#include "MatrixMixerNode.h"

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
    for (auto& m : matrix_)
        m.store (0.0f, std::memory_order_relaxed);

    const int diag = juce::jmin (numIn_, numOut_);
    for (int i = 0; i < diag; ++i)
        matrix_[idx (i, i)].store (1.0f, std::memory_order_relaxed);
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
    scratchIn_.setSize (numIn_, n, false, false, true);
    for (int c = 0; c < numIn_ && c < bufferCh; ++c)
        scratchIn_.copyFrom (c, 0, buffer, c, 0, n);

    for (int outC = 0; outC < numOut_ && outC < bufferCh; ++outC)
    {
        auto* out = buffer.getWritePointer (outC);
        juce::FloatVectorOperations::clear (out, n);

        for (int inC = 0; inC < numIn_; ++inC)
        {
            const float g = matrix_[idx (outC, inC)].load (std::memory_order_relaxed);
            if (g == 0.0f) continue;

            juce::FloatVectorOperations::addWithMultiply (
                out, scratchIn_.getReadPointer (inC), g, n);
        }
    }
}

juce::var MatrixMixerNode::toVar() const
{
    auto* obj = new juce::DynamicObject();

    juce::Array<juce::var> rows;
    rows.ensureStorageAllocated (numOut_);
    for (int o = 0; o < numOut_; ++o)
    {
        juce::Array<juce::var> row;
        row.ensureStorageAllocated (numIn_);
        for (int i = 0; i < numIn_; ++i)
            row.add (matrix_[idx (o, i)].load (std::memory_order_relaxed));
        rows.add (row);
    }
    obj->setProperty ("matrix", rows);

    return juce::var (obj);
}

void MatrixMixerNode::fromVar (const juce::var& v)
{
    if (auto* obj = v.getDynamicObject())
    {
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
}
