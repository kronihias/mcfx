#include "MatrixMixerNode.h"
#include <cstring>

juce::AudioProcessor::BusesProperties MatrixMixerNode::makeBuses (int numIn, int numOut)
{
    return BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (numIn),  true)
        .withOutput ("Output", juce::AudioChannelSet::discreteChannels (numOut), true);
}

MatrixMixerNode::MatrixMixerNode (int numIn, int numOut)
    : juce::AudioProcessor (makeBuses (juce::jmax (1, numIn), juce::jmax (1, numOut))),
      numIn_  (juce::jmax (1, numIn)),
      numOut_ (juce::jmax (1, numOut)),
      matrix_ (numOut_ * numIn_)
{
    resetToIdentity();
}

void MatrixMixerNode::resetToIdentity()
{
    // One store per cell, with its final value: zeroing everything first
    // would let a block render an all-silent matrix in between.
    for (int o = 0; o < numOut_; ++o)
        for (int i = 0; i < numIn_; ++i)
            matrix_[idx (o, i)].store (o == i ? 1.0f : 0.0f, std::memory_order_relaxed);
}

bool MatrixMixerNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().size()  == numIn_
        && layouts.getMainOutputChannelSet().size() == numOut_;
}

void MatrixMixerNode::prepareToPlay (double /*sampleRate*/, int samplesPerBlock)
{
    scratchIn_.setSize (numIn_, samplesPerBlock, false, false, true);
}

void MatrixMixerNode::setMatrix (int outChannel, int inChannel, float gainLinear) noexcept
{
    if (outChannel < 0 || outChannel >= numOut_ || inChannel < 0 || inChannel >= numIn_)
        return;
    matrix_[idx (outChannel, inChannel)].store (gainLinear, std::memory_order_relaxed);
}

float MatrixMixerNode::getMatrix (int outChannel, int inChannel) const noexcept
{
    if (outChannel < 0 || outChannel >= numOut_ || inChannel < 0 || inChannel >= numIn_)
        return 0.0f;
    return matrix_[idx (outChannel, inChannel)].load (std::memory_order_relaxed);
}

void MatrixMixerNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int bufferCh = buffer.getNumChannels();

    // The graph wraps a single in/out bus that's max(numIn, numOut) wide,
    // so the input data sits in channels [0, numIn_) and we'll write
    // outputs to channels [0, numOut_). Snapshot input first.
    //
    // Host blocks larger than announced are mixed in chunks of the prepared
    // scratch size, so nothing allocates on the audio thread.
    const int chunk = scratchIn_.getNumSamples();
    if (chunk <= 0 || scratchIn_.getNumChannels() < numIn_)
    {
        jassertfalse;   // processBlock before prepareToPlay
        buffer.clear();
        return;
    }

    for (int start = 0; start < n; start += chunk)
    {
        const int len = juce::jmin (chunk, n - start);

        for (int c = 0; c < numIn_; ++c)
        {
            if (c < bufferCh) scratchIn_.copyFrom (c, 0, buffer, c, start, len);
            else              scratchIn_.clear (c, 0, len);
        }

        for (int outC = 0; outC < numOut_ && outC < bufferCh; ++outC)
        {
            auto* out = buffer.getWritePointer (outC, start);
            juce::FloatVectorOperations::clear (out, len);

            for (int inC = 0; inC < numIn_; ++inC)
            {
                const float g = matrix_[idx (outC, inC)].load (std::memory_order_relaxed);
                if (g == 0.0f) continue;

                juce::FloatVectorOperations::addWithMultiply (
                    out, scratchIn_.getReadPointer (inC), g, len);
            }
        }
    }
}

// Serialised form (the node's "data" object in presets, the host state and
// every undo snapshot):
//   numIn, numOut          matrix size
//   gains: [[out, in, g]]  non-zero cells only, when there are few of them
//                          (routing matrices are usually sparse), or
//   matrixF32: base64      every cell, float32 little-endian, row-major
//                          (out, in), when the matrix is dense.
// Either way a 256 x 256 matrix is a few KB / one string instead of 65,536
// nested vars, which made every undo snapshot slow and memory hungry.
// fromVar() still reads the old nested "matrix": [[...], ...] arrays.
juce::var MatrixMixerNode::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("numIn",  numIn_);
    obj->setProperty ("numOut", numOut_);

    int nonZero = 0;
    for (const auto& m : matrix_)
        if (m.load (std::memory_order_relaxed) != 0.0f)
            ++nonZero;

    if (nonZero <= kMaxSparseCells)
    {
        juce::Array<juce::var> gains;
        gains.ensureStorageAllocated (nonZero);
        for (int o = 0; o < numOut_; ++o)
            for (int i = 0; i < numIn_; ++i)
                if (const float g = matrix_[idx (o, i)].load (std::memory_order_relaxed); g != 0.0f)
                    gains.add (juce::Array<juce::var> { o, i, g });
        obj->setProperty ("gains", gains);
    }
    else
    {
        juce::MemoryBlock mb (matrix_.size() * sizeof (uint32_t));
        auto* dst = static_cast<uint32_t*> (mb.getData());
        for (size_t k = 0; k < matrix_.size(); ++k)
        {
            const float g = matrix_[k].load (std::memory_order_relaxed);
            uint32_t bits;
            std::memcpy (&bits, &g, sizeof (bits));
            dst[k] = juce::ByteOrder::swapIfBigEndian (bits);
        }
        obj->setProperty ("matrixF32", mb.toBase64Encoding());
    }

    return juce::var (obj);
}

void MatrixMixerNode::fromVar (const juce::var& v)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) return;

    // Size the data was saved at. On a channel-count change the node is
    // rebuilt with fromVar (old->toVar()), so only the overlapping region is
    // taken over; cells outside it keep this node's defaults (the identity
    // diagonal), as with the old nested-array format.
    const int srcIn  = (int) obj->getProperty ("numIn");
    const int srcOut = (int) obj->getProperty ("numOut");

    if (const auto& gains = obj->getProperty ("gains"); gains.isArray() && srcIn > 0 && srcOut > 0)
    {
        for (int o = 0; o < juce::jmin (numOut_, srcOut); ++o)
            for (int i = 0; i < juce::jmin (numIn_, srcIn); ++i)
                matrix_[idx (o, i)].store (0.0f, std::memory_order_relaxed);

        for (const auto& cell : *gains.getArray())
        {
            if (auto* c = cell.getArray(); c != nullptr && c->size() >= 3)
            {
                const int o = (int) (*c)[0];
                const int i = (int) (*c)[1];
                if (juce::isPositiveAndBelow (o, numOut_) && juce::isPositiveAndBelow (i, numIn_))
                    matrix_[idx (o, i)].store ((float) (double) (*c)[2], std::memory_order_relaxed);
            }
        }
        return;
    }

    if (const auto b64 = obj->getProperty ("matrixF32").toString(); b64.isNotEmpty() && srcIn > 0 && srcOut > 0)
    {
        juce::MemoryBlock mb;
        if (! mb.fromBase64Encoding (b64)
            || mb.getSize() < (size_t) srcIn * (size_t) srcOut * sizeof (uint32_t))
            return;

        const auto* src = static_cast<const uint32_t*> (mb.getData());
        for (int o = 0; o < juce::jmin (numOut_, srcOut); ++o)
        {
            for (int i = 0; i < juce::jmin (numIn_, srcIn); ++i)
            {
                const uint32_t bits = juce::ByteOrder::swapIfBigEndian (src[(size_t) o * (size_t) srcIn + (size_t) i]);
                float g;
                std::memcpy (&g, &bits, sizeof (g));
                matrix_[idx (o, i)].store (g, std::memory_order_relaxed);
            }
        }
        return;
    }

    // Legacy: "matrix": [[row of out 0], [row of out 1], ...]
    if (auto* rows = obj->getProperty ("matrix").getArray())
    {
        for (int o = 0; o < numOut_ && o < rows->size(); ++o)
        {
            if (auto* row = (*rows)[o].getArray())
            {
                for (int i = 0; i < numIn_ && i < row->size(); ++i)
                    matrix_[idx (o, i)].store ((float) (double) (*row)[i],
                                               std::memory_order_relaxed);
            }
        }
    }
}
