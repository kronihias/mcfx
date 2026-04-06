/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "EqBand.h"

EqBand::EqBand()
{
}

EqBand::~EqBand()
{
    if (convolver_)
    {
        convolver_->StopProc();
        convolver_->Cleanup();
    }
}

void EqBand::setType(EqBandType t)
{
    type_ = t;
    reset();
}

void EqBand::setIIRSubType(IIRSubType st)
{
    iirSubType_ = st;
    if (st == IIRSubType::RawBiquad)
    {
        hasRawCoeffs_ = true;
        // Don't call updateIIRCoefficients — raw coeffs are set externally
    }
    else
    {
        hasRawCoeffs_ = false;
        updateIIRCoefficients();
    }
}

void EqBand::setFrequency(float f)
{
    frequency_ = f;
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setQ(float q)
{
    q_ = q;
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setGainDB(float db)
{
    gainDB_ = db;
    if (!useLinearGain_)
    {
        float lin = Decibels::decibelsToGain(db);
        linearGain_ = invertGain_ ? -lin : lin;
    }
    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();
}

void EqBand::setLinearGain(float g)
{
    linearGain_ = g;
    gainDB_ = Decibels::gainToDecibels(std::abs(g));
}

void EqBand::setUseLinearGain(bool linear)
{
    useLinearGain_ = linear;
    if (!linear)
    {
        // Switching to dB mode: recompute linearGain_ from gainDB_ + invert
        float lin = Decibels::decibelsToGain(gainDB_);
        linearGain_ = invertGain_ ? -lin : lin;
    }
}

void EqBand::setInvertGain(bool inv)
{
    invertGain_ = inv;
    if (!useLinearGain_)
    {
        float lin = Decibels::decibelsToGain(gainDB_);
        linearGain_ = inv ? -lin : lin;
    }
}

bool EqBand::usesCascade() const
{
    return iirSubType_ == IIRSubType::ButterworthLP
        || iirSubType_ == IIRSubType::ButterworthHP
        || iirSubType_ == IIRSubType::CrossoverLP
        || iirSubType_ == IIRSubType::CrossoverHP
        || iirSubType_ == IIRSubType::CrossoverAP;
}

bool EqBand::isBiquadStable(float a1, float a2)
{
    // For a normalized biquad (a0=1), stability requires both poles inside the unit circle.
    // The Jury stability criterion for z^2 + a1*z + a2 = 0 gives:
    //   |a2| < 1
    //   a2 + a1 > -1   (i.e. 1 + a1 + a2 > 0)
    //   a2 - a1 > -1   (i.e. 1 - a1 + a2 > 0)
    // Also reject NaN/Inf.
    if (std::isnan(a1) || std::isnan(a2) || std::isinf(a1) || std::isinf(a2))
        return false;
    if (std::abs(a2) >= 1.f)
        return false;
    if (1.f + a1 + a2 <= 0.f)
        return false;
    if (1.f - a1 + a2 <= 0.f)
        return false;
    return true;
}

bool EqBand::isRawCoeffsStable() const
{
    if (!hasRawCoeffs_) return true;
    return isBiquadStable(rawCoeffs_.a1, rawCoeffs_.a2);
}

void EqBand::setButterworthOrder(int order)
{
    butterworthOrder_ = jlimit(1, 8, order);
    if (type_ == EqBandType::IIR && !hasRawCoeffs_
        && (iirSubType_ == IIRSubType::ButterworthLP || iirSubType_ == IIRSubType::ButterworthHP))
        updateIIRCoefficients();
}

void EqBand::setCrossoverOrder(int lrOrder)
{
    // Clamp to nearest even in [2,16]
    lrOrder = jlimit(2, 16, lrOrder);
    if (lrOrder % 2 != 0) lrOrder += 1;
    crossoverOrder_ = lrOrder;
    if (type_ == EqBandType::IIR && !hasRawCoeffs_
        && (iirSubType_ == IIRSubType::CrossoverLP || iirSubType_ == IIRSubType::CrossoverHP
            || iirSubType_ == IIRSubType::CrossoverAP))
        updateIIRCoefficients();
}

void EqBand::setFIRCoefficients(const std::vector<float>& coeffs)
{
    firCoeffs_ = coeffs;
    firOriginalCoeffs_ = coeffs;
    firOriginalSampleRate_ = 0.0;  // unknown / same as processing
    firState_.resize(coeffs.size(), 0.f);
}

void EqBand::setFIRCoefficientsWithSampleRate(const std::vector<float>& coeffs, double sampleRate)
{
    firOriginalCoeffs_ = coeffs;
    firOriginalSampleRate_ = sampleRate;
    if (prepared_ && firOriginalSampleRate_ > 0.0 && firOriginalSampleRate_ != sampleRate_)
        resampleFIRCoefficients();
    else
    {
        firCoeffs_ = coeffs;
        firState_.resize(coeffs.size(), 0.f);
    }
}

bool EqBand::loadFIRFromFile(const File& file, int channel)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    int numSamples = (int)reader->lengthInSamples;
    int ch = jmin(channel, (int)reader->numChannels - 1);
    if (ch < 0) ch = 0;

    AudioBuffer<float> buffer(reader->numChannels, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, true);

    std::vector<float> coeffs(numSamples);
    auto* src = buffer.getReadPointer(ch);
    std::copy(src, src + numSamples, coeffs.begin());

    firFilePath_ = file.getFullPathName();
    firFileChannel_ = ch;
    setFIRCoefficientsWithSampleRate(coeffs, reader->sampleRate);
    return true;
}

void EqBand::resampleFIRCoefficients()
{
    if (firOriginalCoeffs_.empty() || firOriginalSampleRate_ <= 0.0
        || firOriginalSampleRate_ == sampleRate_)
    {
        firCoeffs_ = firOriginalCoeffs_;
        firState_.resize(firCoeffs_.size(), 0.f);
        return;
    }

    double sr_conv_fact = sampleRate_ / firOriginalSampleRate_;
    int origSize = (int)firOriginalCoeffs_.size();
    int newSize = (int)std::ceil(origSize * sr_conv_fact);

    AudioBuffer<float> origBuf(1, origSize);
    std::copy(firOriginalCoeffs_.begin(), firOriginalCoeffs_.end(),
              origBuf.getWritePointer(0));

    MemoryAudioSource memorySource(origBuf, false);
    ResamplingAudioSource resamplingSource(&memorySource, false, 1);
    resamplingSource.setResamplingRatio(1.0 / sr_conv_fact);
    resamplingSource.prepareToPlay(newSize, sampleRate_);

    AudioBuffer<float> resampledBuf(1, newSize);
    resampledBuf.clear();

    AudioSourceChannelInfo info;
    info.startSample = 0;
    info.numSamples = newSize;
    info.buffer = &resampledBuf;
    resamplingSource.getNextAudioBlock(info);

    // scale to maintain filter gain
    resampledBuf.applyGain((float)(firOriginalSampleRate_ / sampleRate_));

    firCoeffs_.resize(newSize);
    std::copy(resampledBuf.getReadPointer(0),
              resampledBuf.getReadPointer(0) + newSize,
              firCoeffs_.begin());
    firState_.resize(newSize, 0.f);
}

void EqBand::setRawCoefficients(float b0, float b1, float b2, float a0, float a1, float a2)
{
    hasRawCoeffs_ = true;
    rawCoeffs_ = { b0, b1, b2, a0, a1, a2 };

    // Reject zero or NaN a0
    if (a0 == 0.f || std::isnan(a0) || std::isinf(a0))
    {
        rawCoeffs_ = { b0, b1, b2, 1.f, a1, a2 };
    }
    else if (a0 != 1.f)
    {
        // Normalize by a0
        rawCoeffs_.b0 /= a0;
        rawCoeffs_.b1 /= a0;
        rawCoeffs_.b2 /= a0;
        rawCoeffs_.a1 /= a0;
        rawCoeffs_.a2 /= a0;
        rawCoeffs_.a0 = 1.f;
    }

    // Only set up processing coefficients if stable
    if (isBiquadStable(rawCoeffs_.a1, rawCoeffs_.a2))
    {
        iirCoeffs_ = IIRCoefficients(rawCoeffs_.b0, rawCoeffs_.b1, rawCoeffs_.b2,
                                      1.f, rawCoeffs_.a1, rawCoeffs_.a2);
        startSmoothing();
    }
    // If unstable, coefficients are stored but not applied to processing
}

void EqBand::setDelaySamples(int samples)
{
    delaySamples_ = samples;
    if (delaySamples_ > 0)
    {
        delayBuffer_.resize(delaySamples_, 0.f);
        delayWritePos_ = 0;
    }
}

void EqBand::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    prepared_ = false; // suppress smoothing during initial coefficient calc

    // Initialize parameter smoothing (same approach as SmoothIIRFilter)
    const double rampSeconds = 0.020; // 20ms ramp
    smoothFreq_.reset(sampleRate, rampSeconds);
    smoothQ_.reset(sampleRate, rampSeconds);
    smoothGainLin_.reset(sampleRate, rampSeconds);
    smoothFreq_.setCurrentAndTargetValue(frequency_);
    smoothQ_.setCurrentAndTargetValue(q_);
    smoothGainLin_.setCurrentAndTargetValue(Decibels::decibelsToGain(gainDB_));

    if (type_ == EqBandType::IIR && !hasRawCoeffs_)
        updateIIRCoefficients();

    if (type_ == EqBandType::FIR)
    {
        if (!firOriginalCoeffs_.empty() && firOriginalSampleRate_ > 0.0)
            resampleFIRCoefficients();
        else
            firState_.resize(firCoeffs_.size(), 0.f);

        rebuildConvolver();
    }

    if (type_ == EqBandType::Delay && delaySamples_ > 0)
    {
        delayBuffer_.resize(delaySamples_, 0.f);
        delayWritePos_ = 0;
    }

    reset();
    prepared_ = true;
}

void EqBand::syncParametersFrom(const EqBand& source)
{
    // Copy all parameters from the source band
    // but preserve our filter state and SmoothedValue ramp state
    // so the transition is smooth (no click)

    bool needsCoeffUpdate = false;

    if (type_ != source.type_)
    {
        type_ = source.type_;
        needsCoeffUpdate = true;
    }

    enabled_ = source.enabled_;

    if (type_ == EqBandType::IIR)
    {
        if (iirSubType_ != source.iirSubType_)
        {
            iirSubType_ = source.iirSubType_;
            hasRawCoeffs_ = source.hasRawCoeffs_;
            needsCoeffUpdate = true;
        }

        if (source.hasRawCoeffs_)
        {
            rawCoeffs_ = source.rawCoeffs_;
            iirCoeffs_ = source.iirCoeffs_;
            startSmoothing();
        }
        else
        {
            // Copy parameter values — smoothing handles the transition
            frequency_ = source.frequency_;
            q_ = source.q_;
            gainDB_ = source.gainDB_;
            linearGain_ = source.linearGain_;
            butterworthOrder_ = source.butterworthOrder_;
            crossoverOrder_ = source.crossoverOrder_;

            if (needsCoeffUpdate || usesCascade())
            {
                // Structural change or cascade type: recompute coefficients
                // (cascade types use coefficient interpolation)
                updateIIRCoefficients();
            }
            else
            {
                // Single biquad: update display coefficients and set SmoothedValue targets
                // The audio processing loop will smoothly recalculate per-sample
                updateIIRCoefficients();
            }
        }
    }
    else if (type_ == EqBandType::Gain)
    {
        gainDB_ = source.gainDB_;
        linearGain_ = source.linearGain_;
        useLinearGain_ = source.useLinearGain_;
        invertGain_ = source.invertGain_;
    }
    else if (type_ == EqBandType::Delay)
    {
        delaySamples_ = source.delaySamples_;
        if (delaySamples_ > 0 && (int)delayBuffer_.size() < delaySamples_)
        {
            delayBuffer_.resize(delaySamples_, 0.f);
        }
    }
    else if (type_ == EqBandType::FIR)
    {
        firCoeffs_ = source.firCoeffs_;
        firOriginalCoeffs_ = source.firOriginalCoeffs_;
        firOriginalSampleRate_ = source.firOriginalSampleRate_;
        firFilePath_ = source.firFilePath_;
        firFileChannel_ = source.firFileChannel_;
        if (firState_.size() != firCoeffs_.size())
            firState_.resize(firCoeffs_.size(), 0.f);
    }
}

void EqBand::processBlock(float* data, int numSamples)
{
    if (!enabled_)
        return;

    switch (type_)
    {
        case EqBandType::IIR:   applyIIR(data, numSamples); break;
        case EqBandType::FIR:   applyFIR(data, numSamples); break;
        case EqBandType::Gain:  applyGain(data, numSamples); break;
        case EqBandType::Delay: applyDelay(data, numSamples); break;
    }
}

void EqBand::reset()
{
    iirWork_.resetState();
    for (auto& sec : cascadeWork_)
        sec.resetState();
    smoothSamplesLeft_ = 0;
    std::fill(firState_.begin(), firState_.end(), 0.f);
    std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.f);
    delayWritePos_ = 0;
}

void EqBand::updateIIRCoefficients()
{
    float gain = Decibels::decibelsToGain(gainDB_);
    float f = jlimit(20.f, (float)(sampleRate_ * 0.499), frequency_);

    switch (iirSubType_)
    {
        case IIRSubType::LowPass:
            iirCoeffs_ = IIRCoefficients::makeLowPass(sampleRate_, f, q_);
            break;
        case IIRSubType::HighPass:
            iirCoeffs_ = IIRCoefficients::makeHighPass(sampleRate_, f, q_);
            break;
        case IIRSubType::BandPass:
            iirCoeffs_ = IIRCoefficients::makeBandPass(sampleRate_, f, q_);
            break;
        case IIRSubType::Notch:
            iirCoeffs_ = IIRCoefficients::makeNotchFilter(sampleRate_, f, q_);
            break;
        case IIRSubType::AllPass:
            iirCoeffs_ = IIRCoefficients::makeAllPass(sampleRate_, f, q_);
            break;
        case IIRSubType::LowShelf:
            iirCoeffs_ = IIRCoefficients::makeLowShelf(sampleRate_, f, q_, gain);
            break;
        case IIRSubType::HighShelf:
            iirCoeffs_ = IIRCoefficients::makeHighShelf(sampleRate_, f, q_, gain);
            break;
        case IIRSubType::Peak:
            iirCoeffs_ = IIRCoefficients::makePeakFilter(sampleRate_, f, q_, gain);
            break;

        case IIRSubType::ButterworthLP:
        {
            auto sections = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(f, sampleRate_, butterworthOrder_);
            cascadeCoeffs_.resize(sections.size());
            for (int i = 0; i < (int)sections.size(); ++i)
            {
                auto* c = sections[i]->getRawCoefficients();
                auto order = sections[i]->getFilterOrder();
                if (order == 1)
                    cascadeCoeffs_[i] = { c[0], c[1], 0.f, c[2], 0.f };
                else
                    cascadeCoeffs_[i] = { c[0], c[1], c[2], c[3], c[4] };
            }
            break;
        }

        case IIRSubType::ButterworthHP:
        {
            auto sections = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(f, sampleRate_, butterworthOrder_);
            cascadeCoeffs_.resize(sections.size());
            for (int i = 0; i < (int)sections.size(); ++i)
            {
                auto* c = sections[i]->getRawCoefficients();
                auto order = sections[i]->getFilterOrder();
                if (order == 1)
                    cascadeCoeffs_[i] = { c[0], c[1], 0.f, c[2], 0.f };
                else
                    cascadeCoeffs_[i] = { c[0], c[1], c[2], c[3], c[4] };
            }
            break;
        }

        case IIRSubType::CrossoverLP:
        {
            int bwOrder = crossoverOrder_ / 2;
            auto sections = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(f, sampleRate_, bwOrder);
            int n = (int)sections.size();
            cascadeCoeffs_.resize(n * 2);
            for (int i = 0; i < n; ++i)
            {
                auto* c = sections[i]->getRawCoefficients();
                auto order = sections[i]->getFilterOrder();
                std::array<float, 5> sec;
                if (order == 1)
                    sec = { c[0], c[1], 0.f, c[2], 0.f };
                else
                    sec = { c[0], c[1], c[2], c[3], c[4] };
                cascadeCoeffs_[i] = sec;
                cascadeCoeffs_[i + n] = sec;
            }
            break;
        }

        case IIRSubType::CrossoverHP:
        {
            int bwOrder = crossoverOrder_ / 2;
            auto sections = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(f, sampleRate_, bwOrder);
            int n = (int)sections.size();
            cascadeCoeffs_.resize(n * 2);
            for (int i = 0; i < n; ++i)
            {
                auto* c = sections[i]->getRawCoefficients();
                auto order = sections[i]->getFilterOrder();
                std::array<float, 5> sec;
                if (order == 1)
                    sec = { c[0], c[1], 0.f, c[2], 0.f };
                else
                    sec = { c[0], c[1], c[2], c[3], c[4] };
                cascadeCoeffs_[i] = sec;
                cascadeCoeffs_[i + n] = sec;
            }
            // LR2, LR6, etc. (odd BW half-order): invert HP to sum flat
            if (bwOrder % 2 != 0)
            {
                cascadeCoeffs_[0][0] = -cascadeCoeffs_[0][0];
                cascadeCoeffs_[0][1] = -cascadeCoeffs_[0][1];
                cascadeCoeffs_[0][2] = -cascadeCoeffs_[0][2];
            }
            break;
        }

        case IIRSubType::CrossoverAP:
        {
            int bwOrder = crossoverOrder_ / 2;
            auto sections = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(f, sampleRate_, bwOrder);
            int n = (int)sections.size();
            cascadeCoeffs_.resize(n);
            for (int i = 0; i < n; ++i)
            {
                auto* c = sections[i]->getRawCoefficients();
                auto order = sections[i]->getFilterOrder();
                if (order == 1)
                {
                    float a1 = c[2];
                    cascadeCoeffs_[i] = { a1, 1.0f, 0.f, a1, 0.f };
                }
                else
                {
                    float a1 = c[3], a2 = c[4];
                    cascadeCoeffs_[i] = { a2, a1, 1.0f, a1, a2 };
                }
            }
            break;
        }

        case IIRSubType::RawBiquad:
            // Raw biquad coefficients are set externally via setRawCoefficients()
            return;
    }

    startSmoothing();
}

void EqBand::startSmoothing()
{
    if (usesCascade())
    {
        // Cascade types: coefficient interpolation (recomputing FilterDesign per-sample
        // is too expensive, so we linearly ramp the modified TDF-II coefficients)
        int newSize = (int)cascadeCoeffs_.size();
        cascadeTarget_.resize(newSize);
        for (int i = 0; i < newSize; ++i)
        {
            auto& s = cascadeCoeffs_[i];
            cascadeTarget_[i].setFromStandard(s[0], s[1], s[2], s[3], s[4]);
        }

        if ((int)cascadeWork_.size() != newSize || !prepared_)
        {
            // Section count changed or first init — snap immediately, reset state
            cascadeWork_.resize(newSize);
            for (int i = 0; i < newSize; ++i)
            {
                cascadeWork_[i] = cascadeTarget_[i];
                cascadeWork_[i].resetState();
            }
            smoothSamplesLeft_ = 0;
        }
        else
        {
            // Same topology — smoothly ramp coefficients
            smoothSamplesLeft_ = kSmoothRampSamples;
        }
    }
    else
    {
        // Single biquad types: parameter smoothing via SmoothedValue
        // (same approach as SmoothIIRFilter — smooth parameters, recalculate
        // coefficients each sample from smoothed values)
        if (!prepared_)
        {
            // First init — snap immediately
            smoothFreq_.setCurrentAndTargetValue(frequency_);
            smoothQ_.setCurrentAndTargetValue(q_);
            smoothGainLin_.setCurrentAndTargetValue(Decibels::decibelsToGain(gainDB_));
            recalcWorkingCoeffs();
        }
        else
        {
            // Set targets — SmoothedValues will ramp during processing
            smoothFreq_.setTargetValue(frequency_);
            smoothQ_.setTargetValue(q_);
            smoothGainLin_.setTargetValue(Decibels::decibelsToGain(gainDB_));
        }
    }
}

void EqBand::recalcWorkingCoeffs()
{
    // Read current (possibly smoothed) parameter values and compute
    // modified TDF-II coefficients — called per-sample during smoothing
    float f = smoothFreq_.getNextValue();
    float q = smoothQ_.getNextValue();
    float g = smoothGainLin_.getNextValue();
    f = jlimit(20.f, (float)(sampleRate_ * 0.499), f);

    IIRCoefficients c;
    switch (iirSubType_)
    {
        case IIRSubType::LowPass:   c = IIRCoefficients::makeLowPass(sampleRate_, f, q); break;
        case IIRSubType::HighPass:  c = IIRCoefficients::makeHighPass(sampleRate_, f, q); break;
        case IIRSubType::BandPass:  c = IIRCoefficients::makeBandPass(sampleRate_, f, q); break;
        case IIRSubType::Notch:     c = IIRCoefficients::makeNotchFilter(sampleRate_, f, q); break;
        case IIRSubType::AllPass:   c = IIRCoefficients::makeAllPass(sampleRate_, f, q); break;
        case IIRSubType::LowShelf:  c = IIRCoefficients::makeLowShelf(sampleRate_, f, q, g); break;
        case IIRSubType::HighShelf: c = IIRCoefficients::makeHighShelf(sampleRate_, f, q, g); break;
        case IIRSubType::Peak:      c = IIRCoefficients::makePeakFilter(sampleRate_, f, q, g); break;
        default: return; // cascade types and raw biquad handled separately
    }
    iirWork_.setFromStandard(c.coefficients[0], c.coefficients[1], c.coefficients[2],
                              c.coefficients[3], c.coefficients[4]);
}

void EqBand::applyIIR(float* data, int numSamples)
{
    // Skip processing for unstable raw coefficients (passthrough)
    if (hasRawCoeffs_ && !isBiquadStable(rawCoeffs_.a1, rawCoeffs_.a2))
        return;

    if (usesCascade())
    {
        applyCascadeIIR(data, numSamples);
        return;
    }

    // Modified TDF-II processing with per-sample parameter smoothing
    // (same approach as SmoothIIRFilter: smooth params, recalc coefficients each sample)
    for (int i = 0; i < numSamples; ++i)
    {
        if (smoothFreq_.isSmoothing() || smoothQ_.isSmoothing() || smoothGainLin_.isSmoothing())
            recalcWorkingCoeffs();

        data[i] = iirWork_.process(data[i]);
    }
}

void EqBand::applyCascadeIIR(float* data, int numSamples)
{
    int nSections = (int)cascadeWork_.size();

    for (int i = 0; i < numSamples; ++i)
    {
        // Per-sample coefficient smoothing
        if (smoothSamplesLeft_ > 0)
        {
            float t = 1.f / (float)smoothSamplesLeft_;
            for (int s = 0; s < nSections; ++s)
            {
                auto& w = cascadeWork_[s];
                auto& tgt = cascadeTarget_[s];
                w.b0 += (tgt.b0 - w.b0) * t;
                w.a1 += (tgt.a1 - w.a1) * t;
                w.a2 += (tgt.a2 - w.a2) * t;
                w.c1 += (tgt.c1 - w.c1) * t;
                w.c2 += (tgt.c2 - w.c2) * t;
            }
            --smoothSamplesLeft_;
        }

        // Process through cascade
        float x = data[i];
        for (int s = 0; s < nSections; ++s)
            x = cascadeWork_[s].process(x);
        data[i] = x;
    }
}

void EqBand::rebuildConvolver()
{
    // Tear down existing convolver
    if (convolver_)
    {
        convolver_->StopProc();
        convolver_->Cleanup();
        convolver_.reset();
    }
    useConvolver_ = false;
    convolverLatency_ = 0;

    if (firCoeffs_.empty() || maxBlockSize_ <= 0)
        return;

    int firLen = (int)firCoeffs_.size();
    if (firLen <= maxBlockSize_)
        return;  // short FIR — direct convolution is cheaper and zero-latency

    convolver_ = std::make_unique<MtxConvMaster>();
    int maxPart = jmax(8192, maxBlockSize_);
    if (!convolver_->Configure(1, 1, maxBlockSize_, firLen, maxBlockSize_, maxPart))
    {
        convolver_.reset();
        return;
    }

    AudioSampleBuffer irBuf(1, firLen);
    std::copy(firCoeffs_.begin(), firCoeffs_.end(), irBuf.getWritePointer(0));
    convolver_->AddFilter(0, 0, irBuf);
    convolver_->StartProc();

    convolverIn_.setSize(1, maxBlockSize_);
    convolverOut_.setSize(1, maxBlockSize_);
    convolverIn_.clear();
    convolverOut_.clear();
    useConvolver_ = true;
    convolverLatency_ = maxBlockSize_;
}

void EqBand::applyFIR(float* data, int numSamples)
{
    if (firCoeffs_.empty())
        return;

    // Use partitioned convolution for long FIR filters
    if (useConvolver_ && convolver_)
    {
        std::copy(data, data + numSamples, convolverIn_.getWritePointer(0));
        convolver_->processBlock(convolverIn_, convolverOut_, numSamples);
        std::copy(convolverOut_.getReadPointer(0),
                  convolverOut_.getReadPointer(0) + numSamples,
                  data);
        return;
    }

    // Direct-form time-domain convolution for short filters
    int firLen = (int)firCoeffs_.size();

    for (int i = 0; i < numSamples; ++i)
    {
        // Shift state
        for (int j = firLen - 1; j > 0; --j)
            firState_[j] = firState_[j - 1];
        firState_[0] = data[i];

        float y = 0.f;
        for (int j = 0; j < firLen; ++j)
            y += firCoeffs_[j] * firState_[j];

        data[i] = y;
    }
}

void EqBand::applyGain(float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        data[i] *= linearGain_;
}

void EqBand::applyDelay(float* data, int numSamples)
{
    if (delaySamples_ <= 0 || delayBuffer_.empty())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = data[i];
        int readPos = delayWritePos_ - delaySamples_;
        if (readPos < 0)
            readPos += delaySamples_;
        data[i] = delayBuffer_[readPos];
        delayBuffer_[delayWritePos_] = x;
        delayWritePos_ = (delayWritePos_ + 1) % delaySamples_;
    }
}

std::complex<float> EqBand::getFrequencyResponse(double freqHz, bool alwaysCompute) const
{
    if (!enabled_ && !alwaysCompute)
        return std::complex<float>(1.f, 0.f);

    switch (type_)
    {
        case EqBandType::IIR:
        {
            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            std::complex<double> z_inv(cos(-omega), sin(-omega));
            std::complex<double> z_inv2 = z_inv * z_inv;

            if (usesCascade())
            {
                // Cascade: product of all sections' H(z)
                std::complex<double> result(1.0, 0.0);
                for (auto& sec : cascadeCoeffs_)
                {
                    std::complex<double> num = (double)sec[0] + (double)sec[1] * z_inv + (double)sec[2] * z_inv2;
                    std::complex<double> den = 1.0 + (double)sec[3] * z_inv + (double)sec[4] * z_inv2;
                    result *= num / den;
                }
                return std::complex<float>((float)result.real(), (float)result.imag());
            }

            // Single biquad: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
            auto* c = iirCoeffs_.coefficients;
            float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];

            std::complex<double> num = (double)b0 + (double)b1 * z_inv + (double)b2 * z_inv2;
            std::complex<double> den = 1.0 + (double)a1 * z_inv + (double)a2 * z_inv2;

            auto resp = num / den;
            return std::complex<float>((float)resp.real(), (float)resp.imag());
        }

        case EqBandType::FIR:
        {
            // H(z) = sum(h[n] * z^-n)
            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            std::complex<double> sum(0.0, 0.0);
            for (int n = 0; n < (int)firCoeffs_.size(); ++n)
            {
                double angle = -omega * n;
                sum += (double)firCoeffs_[n] * std::complex<double>(cos(angle), sin(angle));
            }
            return std::complex<float>((float)sum.real(), (float)sum.imag());
        }

        case EqBandType::Gain:
            return std::complex<float>(linearGain_, 0.f);

        case EqBandType::Delay:
        {
            double omega = 2.0 * MathConstants<double>::pi * freqHz / sampleRate_;
            double angle = -omega * delaySamples_;
            return std::complex<float>((float)cos(angle), (float)sin(angle));
        }
    }

    return std::complex<float>(1.f, 0.f);
}

// --- JSON serialization ---

static String iirSubTypeToString(IIRSubType st)
{
    switch (st)
    {
        case IIRSubType::LowPass:   return "low_pass";
        case IIRSubType::HighPass:  return "high_pass";
        case IIRSubType::BandPass:  return "band_pass";
        case IIRSubType::Notch:     return "notch";
        case IIRSubType::AllPass:   return "all_pass";
        case IIRSubType::LowShelf:  return "low_shelf";
        case IIRSubType::HighShelf:     return "high_shelf";
        case IIRSubType::Peak:          return "peak";
        case IIRSubType::ButterworthLP: return "butterworth_lp";
        case IIRSubType::ButterworthHP: return "butterworth_hp";
        case IIRSubType::CrossoverLP:   return "crossover_lp";
        case IIRSubType::CrossoverHP:   return "crossover_hp";
        case IIRSubType::CrossoverAP:   return "crossover_ap";
        case IIRSubType::RawBiquad:     return "raw_biquad";
    }
    return "peak";
}

static IIRSubType stringToIIRSubType(const String& s)
{
    if (s == "low_pass")   return IIRSubType::LowPass;
    if (s == "high_pass")  return IIRSubType::HighPass;
    if (s == "band_pass")  return IIRSubType::BandPass;
    if (s == "notch")      return IIRSubType::Notch;
    if (s == "all_pass")   return IIRSubType::AllPass;
    if (s == "low_shelf")      return IIRSubType::LowShelf;
    if (s == "high_shelf")     return IIRSubType::HighShelf;
    if (s == "butterworth_lp") return IIRSubType::ButterworthLP;
    if (s == "butterworth_hp") return IIRSubType::ButterworthHP;
    if (s == "crossover_lp")  return IIRSubType::CrossoverLP;
    if (s == "crossover_hp")  return IIRSubType::CrossoverHP;
    if (s == "crossover_ap")  return IIRSubType::CrossoverAP;
    if (s == "raw_biquad")   return IIRSubType::RawBiquad;
    return IIRSubType::Peak;
}

var EqBand::toJson() const
{
    auto* obj = new DynamicObject();

    if (diagonal_)
        obj->setProperty("diagonal", true);
    else
    {
        obj->setProperty("input_channel", inputChannel_);
        obj->setProperty("output_channel", outputChannel_);
    }

    {
        auto* params = new DynamicObject();
        switch (type_)
        {
            case EqBandType::IIR:
                params->setProperty("type", iirSubTypeToString(iirSubType_));
                if (iirSubType_ == IIRSubType::RawBiquad)
                {
                    params->setProperty("b0", rawCoeffs_.b0);
                    params->setProperty("b1", rawCoeffs_.b1);
                    params->setProperty("b2", rawCoeffs_.b2);
                    params->setProperty("a0", rawCoeffs_.a0);
                    params->setProperty("a1", rawCoeffs_.a1);
                    params->setProperty("a2", rawCoeffs_.a2);
                }
                else
                {
                    params->setProperty("f_Hz", frequency_);
                    if (iirSubType_ == IIRSubType::ButterworthLP || iirSubType_ == IIRSubType::ButterworthHP)
                        params->setProperty("order", butterworthOrder_);
                    else if (iirSubType_ == IIRSubType::CrossoverLP || iirSubType_ == IIRSubType::CrossoverHP
                             || iirSubType_ == IIRSubType::CrossoverAP)
                        params->setProperty("order", crossoverOrder_);
                    else
                    {
                        params->setProperty("Q", q_);
                        params->setProperty("gain_db", gainDB_);
                    }
                }
                break;
            case EqBandType::FIR:
            {
                params->setProperty("type", "fir");
                // Use original (unresampled) coefficients for storage
                const auto& coeffsToStore = firOriginalCoeffs_.empty() ? firCoeffs_ : firOriginalCoeffs_;
                if (firOriginalSampleRate_ > 0.0)
                    params->setProperty("sample_rate", firOriginalSampleRate_);
                if (firFilePath_.isNotEmpty())
                {
                    params->setProperty("file", firFilePath_);
                    params->setProperty("channel", firFileChannel_);
                }
                if (coeffsToStore.size() > 64)
                {
                    // Base64 encoding for large FIR filters
                    MemoryBlock mb(coeffsToStore.data(), coeffsToStore.size() * sizeof(float));
                    params->setProperty("coefficients_b64", mb.toBase64Encoding());
                    params->setProperty("coefficients_b64_format", "float32le");
                }
                else
                {
                    Array<var> arr;
                    for (auto c : coeffsToStore)
                        arr.add(c);
                    params->setProperty("coefficients", arr);
                }
                break;
            }
            case EqBandType::Gain:
                params->setProperty("type", "gain");
                if (useLinearGain_)
                {
                    params->setProperty("gain_linear", (double)linearGain_);
                }
                else
                {
                    params->setProperty("gain_db", gainDB_);
                    if (invertGain_)
                        params->setProperty("invert", true);
                }
                break;
            case EqBandType::Delay:
                params->setProperty("type", "delay");
                params->setProperty("delay_samples", delaySamples_);
                break;
        }
        obj->setProperty("parameters", var(params));
    }

    if (!enabled_)
        obj->setProperty("enabled", false);

    return var(obj);
}

EqBand* EqBand::fromJson(const var& json)
{
    auto* band = new EqBand();

    // Channel routing
    if (json.hasProperty("diagonal") && (bool)json["diagonal"])
    {
        band->setDiagonal(true);
    }
    else
    {
        band->setDiagonal(false);
        if (json.hasProperty("input_channel"))
            band->setInputChannel((int)json["input_channel"]);
        if (json.hasProperty("output_channel"))
            band->setOutputChannel((int)json["output_channel"]);
    }

    // Raw biquad coefficients
    if (json.hasProperty("coefficients"))
    {
        auto coeffs = json["coefficients"];
        band->setType(EqBandType::IIR);
        band->setRawCoefficients(
            (float)coeffs.getProperty("b0", 1.0),
            (float)coeffs.getProperty("b1", 0.0),
            (float)coeffs.getProperty("b2", 0.0),
            (float)coeffs.getProperty("a0", 1.0),
            (float)coeffs.getProperty("a1", 0.0),
            (float)coeffs.getProperty("a2", 0.0)
        );
    }
    // Parameterized
    else if (json.hasProperty("parameters"))
    {
        auto params = json["parameters"];
        String type = params.getProperty("type", "peak").toString();

        if (type == "gain")
        {
            band->setType(EqBandType::Gain);
            if (params.hasProperty("gain_linear"))
            {
                band->setUseLinearGain(true);
                band->setLinearGain((float)(double)params["gain_linear"]);
            }
            else
            {
                band->setUseLinearGain(false);
                band->setGainDB((float)params.getProperty("gain_db", 0.0));
                if (params.hasProperty("invert") && (bool)params["invert"])
                    band->setInvertGain(true);
            }
        }
        else if (type == "delay")
        {
            band->setType(EqBandType::Delay);
            band->setDelaySamples((int)params.getProperty("delay_samples", 0));
        }
        else if (type == "fir")
        {
            band->setType(EqBandType::FIR);
            double firSR = (double)params.getProperty("sample_rate", 0.0);
            String filePath = params.getProperty("file", "").toString();
            int fileChannel = (int)params.getProperty("channel", 0);

            bool loaded = false;

            // Try loading from WAV/audio file first
            if (filePath.isNotEmpty())
            {
                File f(filePath);
                if (f.existsAsFile())
                    loaded = band->loadFIRFromFile(f, fileChannel);
            }

            if (!loaded)
            {
                std::vector<float> coeffs;

                // Try base64-encoded coefficients
                if (params.hasProperty("coefficients_b64"))
                {
                    MemoryBlock mb;
                    if (mb.fromBase64Encoding(params["coefficients_b64"].toString()))
                    {
                        int numFloats = (int)(mb.getSize() / sizeof(float));
                        coeffs.resize(numFloats);
                        std::memcpy(coeffs.data(), mb.getData(), numFloats * sizeof(float));
                    }
                }
                // Fallback: JSON array
                else
                {
                    auto arr = params["coefficients"];
                    if (arr.isArray())
                    {
                        for (int i = 0; i < arr.size(); ++i)
                            coeffs.push_back((float)arr[i]);
                    }
                }

                if (!coeffs.empty())
                {
                    if (firSR > 0.0)
                        band->setFIRCoefficientsWithSampleRate(coeffs, firSR);
                    else
                        band->setFIRCoefficients(coeffs);
                }

                // Remember file path even if file wasn't found (for portability)
                if (filePath.isNotEmpty())
                {
                    band->setFIRFilePath(filePath);
                    band->setFIRFileChannel(fileChannel);
                }
            }
        }
        else
        {
            // IIR parameterized
            band->setType(EqBandType::IIR);
            auto subType = stringToIIRSubType(type);
            band->setIIRSubType(subType);
            if (subType == IIRSubType::RawBiquad)
            {
                band->setRawCoefficients(
                    (float)params.getProperty("b0", 1.0),
                    (float)params.getProperty("b1", 0.0),
                    (float)params.getProperty("b2", 0.0),
                    (float)params.getProperty("a0", 1.0),
                    (float)params.getProperty("a1", 0.0),
                    (float)params.getProperty("a2", 0.0)
                );
            }
            else
            {
                band->setFrequency((float)params.getProperty("f_Hz", 1000.0));
                if (subType == IIRSubType::ButterworthLP || subType == IIRSubType::ButterworthHP)
                    band->setButterworthOrder((int)params.getProperty("order", 2));
                else if (subType == IIRSubType::CrossoverLP || subType == IIRSubType::CrossoverHP
                         || subType == IIRSubType::CrossoverAP)
                    band->setCrossoverOrder((int)params.getProperty("order", 4));
                else
                {
                    band->setQ((float)params.getProperty("Q", 0.707));
                    band->setGainDB((float)params.getProperty("gain_db", 0.0));
                }
            }
        }
    }

    if (json.hasProperty("enabled"))
        band->setEnabled((bool)json["enabled"]);

    return band;
}
