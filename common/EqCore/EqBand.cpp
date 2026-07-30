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
#include "AnalogPrototypeDesigner.h"

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

void EqBand::setTiltLoHz(float hz)
{
    tiltLoHz_ = jlimit(10.f, 2000.f, hz);
    if (type_ == EqBandType::IIR && iirSubType_ == IIRSubType::Tilt)
        updateIIRCoefficients();
}

void EqBand::setTiltHiHz(float hz)
{
    tiltHiHz_ = jlimit(1000.f, 24000.f, hz);
    if (type_ == EqBandType::IIR && iirSubType_ == IIRSubType::Tilt)
        updateIIRCoefficients();
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

// --- Dynamic EQ ---

bool EqBand::supportsDynamic() const
{
    if (hasRawCoeffs_)
        return false;
    if (type_ == EqBandType::Gain)          // broadband compressor (full-band detector)
        return true;
    if (type_ != EqBandType::IIR)
        return false;
    return iirSubType_ == IIRSubType::Peak
        || iirSubType_ == IIRSubType::LowShelf
        || iirSubType_ == IIRSubType::HighShelf;
}

bool EqBand::isDynamicBroadband() const
{
    return type_ == EqBandType::Gain;       // Gain bands detect on the full-band level
}

IIRCoefficients EqBand::makeDetectorCoeffs(IIRSubType subType, double sampleRate,
                                           float freq, float q)
{
    float f = jlimit(20.f, (float)(sampleRate * 0.499), freq);
    switch (subType)
    {
        case IIRSubType::LowShelf:  return IIRCoefficients::makeLowPass (sampleRate, f, q);
        case IIRSubType::HighShelf: return IIRCoefficients::makeHighPass(sampleRate, f, q);
        default:                    return IIRCoefficients::makeBandPass(sampleRate, f, q);
    }
}

void EqBand::designTiltCascade()
{
    // Tilt = a straight line on a dB vs log-frequency plot: constant slope in
    // dB/octave, crossing 0 dB at the band's frequency (the pivot).
    //
    // Built from kTiltSections first-order pole/zero pairs on a grid that is
    // geometric in the bilinear-warped variable W = tan(pi*f/fs) rather than in Hz.
    // Working in W is what keeps the line straight in the top octaves: the grid can
    // run past Nyquist (W -> infinity), and each section's height is taken from the
    // target's own increment across its interval, so the frequency warping is
    // absorbed instead of steepening the treble.
    //
    // A section with pole Wp and zero Wz contributes a shelf of 20*log10(Wp/Wz) dB;
    // straddling the grid point (Wp,Wz = Wc*10^(+/-h/40)) makes rising and falling
    // slopes exactly symmetric. Asymptotic heights alone still droop at HF — the
    // magnitude slope of any real filter has to vanish at Nyquist — so a few
    // fixed-point passes re-fit the heights against the realised curve, which takes
    // the worst case from ~1.2 dB down to <0.2 dB over 20 Hz..20 kHz at any rate.
    const double fs = sampleRate_ > 0.0 ? sampleRate_ : 48000.0;
    const double slope = jlimit(-(double) kTiltMaxSlope, (double) kTiltMaxSlope,
                                (double) gainDB_);          // dB per octave

    double W[kTiltSections + 1], fd[kTiltSections + 1], tgt[kTiltSections + 1];
    double h[kTiltSections];

    const double wLo = std::tan(MathConstants<double>::pi * kTiltLowHz / fs);
    const double stepRatio = std::pow(kTiltWarpHi / wLo, 1.0 / (double) kTiltSections);
    for (int k = 0; k <= kTiltSections; ++k)
    {
        W[k]  = wLo * std::pow(stepRatio, (double) k);
        fd[k] = fs / MathConstants<double>::pi * std::atan(W[k]);   // grid edge, in Hz
    }
    // Bound the line: outside [lo, hi] the target stops changing, which drives the
    // sections there to unit gain on its own — a section's height is just the
    // target's increment across its interval, and that is now zero. The corners
    // come out rounded, since a cascade this smooth cannot follow a kink; that is
    // the shape a shelf wants anyway.
    const double lo = jlimit(10.0, 0.4 * fs, (double) tiltLoHz_);
    const double hi = jmax(lo * 1.05, jlimit(10.0, 0.45 * fs, (double) tiltHiHz_));
    for (int k = 0; k <= kTiltSections; ++k)
        tgt[k] = slope * std::log2(jlimit(lo, hi, fd[k]) / fd[0]);
    for (int k = 0; k < kTiltSections; ++k)
        h[k] = tgt[k + 1] - tgt[k];

    auto build = [&]
    {
        cascadeCoeffs_.resize((size_t) kTiltSections);
        for (int k = 0; k < kTiltSections; ++k)
        {
            const double Wc = std::sqrt(W[k] * W[k + 1]);
            const double Wp = Wc * std::pow(10.0,  h[k] / 40.0);
            const double Wz = Wc * std::pow(10.0, -h[k] / 40.0);
            const double den = 1.0 + 1.0 / Wp;
            cascadeCoeffs_[(size_t) k] = { (float) ((1.0 + 1.0 / Wz) / den),
                                           (float) ((1.0 - 1.0 / Wz) / den),
                                           0.f,
                                           (float) ((1.0 - 1.0 / Wp) / den),
                                           0.f };
        }
    };

    auto magDB = [&] (double hz)
    {
        const double w = 2.0 * MathConstants<double>::pi * hz / fs;
        const std::complex<double> zi(std::cos(-w), std::sin(-w));
        std::complex<double> H(1.0, 0.0);
        for (auto& s : cascadeCoeffs_)
            H *= ((double) s[0] + (double) s[1] * zi) / (1.0 + (double) s[3] * zi);
        return 20.0 * std::log10(jmax(1.0e-12, std::abs(H)));
    };

    for (int pass = 0; pass < kTiltFitPasses; ++pass)
    {
        build();
        const double ref = magDB(fd[0]);
        double err[kTiltSections + 1];
        for (int k = 0; k <= kTiltSections; ++k)
            err[k] = tgt[k] - (magDB(fd[k]) - ref);
        for (int k = 0; k < kTiltSections; ++k)
            h[k] = jlimit(-18.0, 18.0, h[k] + (err[k + 1] - err[k]));
    }
    build();

    // Slide the whole line so it crosses 0 dB at the pivot.
    const double pivot = jlimit(20.0, 0.45 * fs, (double) frequency_);
    const double g = std::pow(10.0, -magDB(pivot) / 20.0);
    cascadeCoeffs_[0][0] = (float) ((double) cascadeCoeffs_[0][0] * g);
    cascadeCoeffs_[0][1] = (float) ((double) cascadeCoeffs_[0][1] * g);
}

int EqBand::getLookaheadSamples(double sampleRate) const
{
    if (! dynActive_ || ! supportsDynamic() || dynLookaheadMs_ <= 0.f)
        return 0;
    return juce::roundToInt((double) dynLookaheadMs_ * 0.001 * sampleRate);
}

void EqBand::setDynamicActive(bool b)
{
    if (b == dynActive_)
        return;
    dynActive_ = b;
    // Start clean so the offset ramps in from 0 dB (no pop on toggle).
    dynEnv_ = 0.f;
    dynThreshEnv_ = dynThresholdDB_;
    dynCtrlCounter_ = 0;
    dynRampLeft_ = 0;
    dynNeedsSnap_ = true;
    dynDetector_.resetState();
}

void EqBand::updateDynEnvCoeffs()
{
    double sr = sampleRate_ > 0.0 ? sampleRate_ : 48000.0;
    double atkSamples = jmax(1.0, (double)dynAttackMs_  * 0.001 * sr);
    double relSamples = jmax(1.0, (double)dynReleaseMs_ * 0.001 * sr);
    dynAtkCoeff_ = (float)std::exp(-1.0 / atkSamples);
    dynRelCoeff_ = (float)std::exp(-1.0 / relSamples);
}

bool EqBand::usesCascade() const
{
    return iirSubType_ == IIRSubType::ButterworthLP
        || iirSubType_ == IIRSubType::ButterworthHP
        || iirSubType_ == IIRSubType::CrossoverLP
        || iirSubType_ == IIRSubType::CrossoverHP
        || iirSubType_ == IIRSubType::CrossoverAP
        || iirSubType_ == IIRSubType::Chebyshev1LP
        || iirSubType_ == IIRSubType::Chebyshev1HP
        || iirSubType_ == IIRSubType::Chebyshev2LP
        || iirSubType_ == IIRSubType::Chebyshev2HP
        || iirSubType_ == IIRSubType::EllipticLP
        || iirSubType_ == IIRSubType::EllipticHP
        || iirSubType_ == IIRSubType::BesselLP
        || iirSubType_ == IIRSubType::BesselHP
        || iirSubType_ == IIRSubType::Tilt;   // constant-slope line (1st-order pairs)
}

static bool isAnalogPrototypeSubType (IIRSubType st)
{
    return st == IIRSubType::Chebyshev1LP || st == IIRSubType::Chebyshev1HP
        || st == IIRSubType::Chebyshev2LP || st == IIRSubType::Chebyshev2HP
        || st == IIRSubType::EllipticLP   || st == IIRSubType::EllipticHP
        || st == IIRSubType::BesselLP     || st == IIRSubType::BesselHP;
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

void EqBand::setAnalogOrder(int order)
{
    analogOrder_ = jlimit(1, 12, order);
    if (type_ == EqBandType::IIR && !hasRawCoeffs_ && isAnalogPrototypeSubType(iirSubType_))
        updateIIRCoefficients();
}

void EqBand::setRipplePassDB(float db)
{
    ripplePassDB_ = jlimit(0.001f, 12.0f, db);
    if (type_ == EqBandType::IIR && !hasRawCoeffs_
        && (iirSubType_ == IIRSubType::Chebyshev1LP || iirSubType_ == IIRSubType::Chebyshev1HP
            || iirSubType_ == IIRSubType::EllipticLP || iirSubType_ == IIRSubType::EllipticHP))
        updateIIRCoefficients();
}

void EqBand::setRippleStopDB(float db)
{
    rippleStopDB_ = jlimit(20.0f, 120.0f, db);
    if (type_ == EqBandType::IIR && !hasRawCoeffs_
        && (iirSubType_ == IIRSubType::Chebyshev2LP || iirSubType_ == IIRSubType::Chebyshev2HP
            || iirSubType_ == IIRSubType::EllipticLP || iirSubType_ == IIRSubType::EllipticHP))
        updateIIRCoefficients();
}

void EqBand::setFIRCoefficients(const std::vector<float>& coeffs)
{
    firCoeffs_ = coeffs;
    firOriginalCoeffs_ = coeffs;
    originalSampleRate_ = 0.0;  // unknown / same as processing
    firState_.resize(coeffs.size(), 0.f);
    rebuildFIRFrequencyResponse();
    if (prepared_)
        rebuildConvolver();
}

void EqBand::setFIRCoefficientsWithSampleRate(const std::vector<float>& coeffs, double sampleRate)
{
    firOriginalCoeffs_ = coeffs;
    originalSampleRate_ = sampleRate;
    if (prepared_ && originalSampleRate_ > 0.0 && originalSampleRate_ != sampleRate_)
        resampleFIRCoefficients();  // calls rebuildFIRFrequencyResponse() internally
    else
    {
        firCoeffs_ = coeffs;
        firState_.resize(coeffs.size(), 0.f);
        rebuildFIRFrequencyResponse();
    }
    if (prepared_)
        rebuildConvolver();
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
    if (firOriginalCoeffs_.empty() || originalSampleRate_ <= 0.0
        || originalSampleRate_ == sampleRate_)
    {
        firCoeffs_ = firOriginalCoeffs_;
        firState_.resize(firCoeffs_.size(), 0.f);
        return;
    }

    double sr_conv_fact = sampleRate_ / originalSampleRate_;
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
    resampledBuf.applyGain((float)(originalSampleRate_ / sampleRate_));

    firCoeffs_.resize(newSize);
    std::copy(resampledBuf.getReadPointer(0),
              resampledBuf.getReadPointer(0) + newSize,
              firCoeffs_.begin());
    firState_.resize(newSize, 0.f);
    rebuildFIRFrequencyResponse();
}

void EqBand::rebuildFIRFrequencyResponse()
{
    if (firCoeffs_.empty())
    {
        firFFT_.clear();
        firFFTSize_ = 0;
        return;
    }

    // Choose FFT size: next power of 2 >= firCoeffs_.size(), minimum 1024
    int n = (int)firCoeffs_.size();
    int fftOrder = 0;
    int fftSize = 1;
    while (fftSize < jmax(1024, n))
    {
        fftSize <<= 1;
        fftOrder++;
    }

    // performRealOnlyForwardTransform reads N real samples from the first half
    // of the buffer (fftData[0..N-1]) and writes back ((N/2)+1) complex bins
    // (interleaved real,imag) into the full 2*N-sized buffer.
    std::vector<float> fftData((size_t)(fftSize * 2), 0.f);
    for (int i = 0; i < n; ++i)
        fftData[(size_t) i] = firCoeffs_[(size_t) i];

    juce::dsp::FFT fft(fftOrder);
    fft.performRealOnlyForwardTransform(fftData.data(), true);

    int numBins = fftSize / 2 + 1;
    firFFT_.resize(numBins);
    firFFTSize_ = fftSize;
    for (int i = 0; i < numBins; ++i)
        firFFT_[i] = std::complex<float>(fftData[(size_t) (i * 2)],
                                           fftData[(size_t) (i * 2 + 1)]);
}

void EqBand::setOriginalSampleRate(double sr)
{
    originalSampleRate_ = sr;
    if (!prepared_)
        return;

    if (type_ == EqBandType::IIR && hasRawCoeffs_)
        resampleRawBiquad();
    else if (type_ == EqBandType::FIR && !firOriginalCoeffs_.empty())
    {
        resampleFIRCoefficients();
        rebuildConvolver();
    }
}

void EqBand::resampleRawBiquad()
{
    // Resample raw biquad coefficients from originalSampleRate_ to sampleRate_
    // using inverse bilinear transform → analog prototype → forward bilinear transform.
    if (!hasRawCoeffs_ || originalSampleRate_ <= 0.0 || originalSampleRate_ == sampleRate_)
    {
        // No resampling needed — apply raw coefficients directly
        if (isBiquadStable(rawCoeffs_.a1, rawCoeffs_.a2))
        {
            iirCoeffs_ = IIRCoefficients(rawCoeffs_.b0, rawCoeffs_.b1, rawCoeffs_.b2,
                                          1.f, rawCoeffs_.a1, rawCoeffs_.a2);
            startSmoothing();
        }
        return;
    }

    // rawCoeffs_ stores the user's original (normalized: a0=1) coefficients at originalSampleRate_.
    // We map them to sampleRate_ via the s-domain.
    double c1 = 1.0 / (2.0 * originalSampleRate_);
    double c2 = 1.0 / (2.0 * sampleRate_);

    double b0 = rawCoeffs_.b0, b1 = rawCoeffs_.b1, b2 = rawCoeffs_.b2;
    double a0 = 1.0,           a1 = rawCoeffs_.a1, a2 = rawCoeffs_.a2;

    // Analog prototype via inverse bilinear at originalSampleRate_
    double A0 = b0 + b1 + b2;
    double A1 = 2.0 * c1 * (b0 - b2);
    double A2 = c1 * c1 * (b0 - b1 + b2);
    double B0 = a0 + a1 + a2;
    double B1 = 2.0 * c1 * (a0 - a2);
    double B2 = c1 * c1 * (a0 - a1 + a2);

    // Forward bilinear at sampleRate_
    double c2sq = c2 * c2;
    double nb0 = A0 * c2sq + A1 * c2 + A2;
    double nb1 = 2.0 * A0 * c2sq - 2.0 * A2;
    double nb2 = A0 * c2sq - A1 * c2 + A2;
    double na0 = B0 * c2sq + B1 * c2 + B2;
    double na1 = 2.0 * B0 * c2sq - 2.0 * B2;
    double na2 = B0 * c2sq - B1 * c2 + B2;

    // Normalize by na0
    if (std::abs(na0) < 1e-15)
        return; // degenerate — don't update

    nb0 /= na0; nb1 /= na0; nb2 /= na0;
    na1 /= na0; na2 /= na0;

    if (isBiquadStable((float)na1, (float)na2))
    {
        iirCoeffs_ = IIRCoefficients((float)nb0, (float)nb1, (float)nb2,
                                      1.f, (float)na1, (float)na2);
        startSmoothing();
    }
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

    // Apply coefficients (with possible resampling for raw biquad)
    resampleRawBiquad();
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
    else if (type_ == EqBandType::IIR && hasRawCoeffs_)
        resampleRawBiquad();

    if (type_ == EqBandType::FIR)
    {
        if (!firOriginalCoeffs_.empty() && originalSampleRate_ > 0.0)
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

    // Dynamic EQ state
    updateDynEnvCoeffs();
    dynThreshEnv_ = dynThresholdDB_;
    externalDynOffset_ = nullptr;
    lastDynOffsetDB_ = 0.f;
    if (supportsDynamic())
    {
        auto dc = makeDetectorCoeffs(iirSubType_, sampleRate_, frequency_, q_);
        dynDetector_.setFromStandard(dc.coefficients[0], dc.coefficients[1], dc.coefficients[2],
                                      dc.coefficients[3], dc.coefficients[4]);
    }
    // Size the lookahead main-delay ring from the current lookahead time.
    dynLookaheadSamples_ = getLookaheadSamples(sampleRate_);
    dynMainDelayBuffer_.assign((size_t) jmax(0, dynLookaheadSamples_), 0.f);
    dynMainDelayPos_ = 0;

    reset();
    prepared_ = true;
}

void EqBand::syncParametersFrom(const EqBand& source)
{
    // Copy all parameters from the source band
    // but preserve our filter state and SmoothedValue ramp state
    // so the transition is smooth (no click)

    type_ = source.type_;

    enabled_ = source.enabled_;

    // Dynamic EQ parameters — copy without disturbing detector/envelope state so
    // continuous changes (threshold/range/attack/release/auto) stay click-free.
    // (dynActive / dynLinked changes are routed through a full rebuild, not sync.)
    {
        bool wasActive = dynActive_;
        dynActive_      = source.dynActive_;
        dynThresholdDB_ = source.dynThresholdDB_;
        dynRangeDB_     = source.dynRangeDB_;
        dynAuto_        = source.dynAuto_;
        dynLinked_      = source.dynLinked_;
        dynLookaheadMs_ = source.dynLookaheadMs_;  // realized via rebuild (structural)
        if (dynAttackMs_ != source.dynAttackMs_ || dynReleaseMs_ != source.dynReleaseMs_)
        {
            dynAttackMs_  = source.dynAttackMs_;
            dynReleaseMs_ = source.dynReleaseMs_;
            updateDynEnvCoeffs();
        }
        if (dynActive_ && !wasActive)
        {
            dynEnv_ = 0.f;
            dynThreshEnv_ = dynThresholdDB_;
            dynCtrlCounter_ = 0;
            dynNeedsSnap_ = true;
            dynDetector_.resetState();
        }
    }

    if (type_ == EqBandType::IIR)
    {
        if (iirSubType_ != source.iirSubType_)
        {
            iirSubType_ = source.iirSubType_;
            hasRawCoeffs_ = source.hasRawCoeffs_;
        }

        originalSampleRate_ = source.originalSampleRate_;

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
            analogOrder_   = source.analogOrder_;
            ripplePassDB_  = source.ripplePassDB_;
            rippleStopDB_  = source.rippleStopDB_;
            tiltLoHz_      = source.tiltLoHz_;
            tiltHiHz_      = source.tiltHiHz_;

            if (usesCascade())
            {
                // The cascade designs (the tilt fit, the Butterworth/elliptic
                // prototypes) are far too heavy for the audio thread, and the
                // model band already ran them on the GUI thread when its
                // parameter changed — copy the result, the same way raw
                // coefficients are copied above. Allocation-free once the
                // section count has settled; a count change is a structural
                // edit, for which startSmoothing() snaps rather than ramps.
                cascadeCoeffs_ = source.cascadeCoeffs_;
                startSmoothing();
            }
            else
            {
                // Single biquad: closed-form recompute, sets SmoothedValue
                // targets that the processing loop ramps per-sample.
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
        originalSampleRate_ = source.originalSampleRate_;
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

void EqBand::processBlock(float* data, int numSamples, const float* chainInput)
{
    if (!enabled_)
        return;

    if (dynActive_ && supportsDynamic())
    {
        if (type_ == EqBandType::Gain)
            applyDynamicGain(data, numSamples, chainInput);   // broadband compressor
        else
            applyDynamicIIR(data, numSamples, chainInput);
        return;
    }

    // Non-dynamic band (or unsupported type): static processing.
    processBlock(data, numSamples);
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

    // Dynamic EQ runtime state
    dynDetector_.resetState();
    dynEnv_ = 0.f;
    dynCtrlCounter_ = 0;
    dynRampLeft_ = 0;
    dynNeedsSnap_ = true;
    std::fill(dynMainDelayBuffer_.begin(), dynMainDelayBuffer_.end(), 0.f);
    dynMainDelayPos_ = 0;
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
        case IIRSubType::Tilt:
            designTiltCascade();          // constant-slope line (cascade of 1st-order pairs)
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

        case IIRSubType::Chebyshev1LP:
        case IIRSubType::Chebyshev1HP:
        case IIRSubType::Chebyshev2LP:
        case IIRSubType::Chebyshev2HP:
        case IIRSubType::EllipticLP:
        case IIRSubType::EllipticHP:
        case IIRSubType::BesselLP:
        case IIRSubType::BesselHP:
        {
            using namespace EqDesign;
            Family fam = Family::Bessel;
            switch (iirSubType_)
            {
                case IIRSubType::Chebyshev1LP:
                case IIRSubType::Chebyshev1HP: fam = Family::Chebyshev1; break;
                case IIRSubType::Chebyshev2LP:
                case IIRSubType::Chebyshev2HP: fam = Family::Chebyshev2; break;
                case IIRSubType::EllipticLP:
                case IIRSubType::EllipticHP:   fam = Family::Elliptic;   break;
                default: fam = Family::Bessel; break;
            }
            Mode mode = (iirSubType_ == IIRSubType::Chebyshev1HP
                      || iirSubType_ == IIRSubType::Chebyshev2HP
                      || iirSubType_ == IIRSubType::EllipticHP
                      || iirSubType_ == IIRSubType::BesselHP)
                ? Mode::Highpass : Mode::Lowpass;

            auto sos = designDigitalCascade(fam, mode, analogOrder_, f, sampleRate_,
                                              ripplePassDB_, rippleStopDB_);
            cascadeCoeffs_.resize(sos.size());
            for (int i = 0; i < (int)sos.size(); ++i)
            {
                cascadeCoeffs_[i] = { (float)sos[i].b0, (float)sos[i].b1, (float)sos[i].b2,
                                       (float)sos[i].a1, (float)sos[i].a2 };
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
        default: return; // cascade types (incl. tilt) and raw biquad handled separately
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

void EqBand::applyDynamicIIR(float* data, int numSamples, const float* chainInput)
{
    // Dynamic peak/shelf: a level detector (band-pass/LP/HP at the band's freq/Q)
    // modulates the band gain by an offset O(dB) bounded by Range. The envelope
    // follower runs per-sample; the (transcendental) coefficient recompute runs at
    // control rate and is interpolated per sample (zipper-free, bounded CPU).
    //
    // Lookahead: the detector reads the signal arriving at this band (its local
    // input, undelayed) while the audio is processed through a main delay of
    // dynLookaheadSamples_, so the gain anticipates the audio by that many samples.
    // Linked bands consume externalDynOffset_ (computed upstream, already aligned).
    ignoreUnused(chainInput);   // independent bands self-detect on the local signal
    const bool linked = (externalDynOffset_ != nullptr);
    const bool hasLA  = dynLookaheadSamples_ > 0 && ! dynMainDelayBuffer_.empty();

    const float rngAbs  = std::abs(dynRangeDB_);
    const float rngSign = (dynRangeDB_ < 0.f) ? -1.f : 1.f;

    // Auto-threshold tracking: one-pole over the detection level, ~1.5 s, stepped
    // once per control block.
    const float autoCoeff = (float)std::exp(-(double)kDynCtrlSamples
                                            / (1.5 * jmax(1.0, sampleRate_)));

    for (int i = 0; i < numSamples; ++i)
    {
        // Advance parameter smoothing so freq/Q/static-gain edits still ramp.
        float f    = smoothFreq_.getNextValue();
        float q    = smoothQ_.getNextValue();
        float gLin = smoothGainLin_.getNextValue();

        float localIn = data[i];   // signal arriving at this band (detection + lookahead source)

        // --- Per-sample detection (independent mode self-detects on local signal) ---
        if (!linked)
        {
            float bp = dynDetector_.process(localIn);
            float p  = bp * bp;
            float c  = (p > dynEnv_) ? dynAtkCoeff_ : dynRelCoeff_;
            dynEnv_  = c * dynEnv_ + (1.f - c) * p;
        }

        // --- Control-rate coefficient recompute ---
        if (dynCtrlCounter_ <= 0)
        {
            dynCtrlCounter_ = kDynCtrlSamples;

            float O;
            if (linked)
            {
                O = externalDynOffset_[i];
            }
            else
            {
                float lvlDB = 10.f * std::log10(dynEnv_ + 1e-12f);
                float thr;
                if (dynAuto_)
                {
                    dynThreshEnv_ = autoCoeff * dynThreshEnv_ + (1.f - autoCoeff) * lvlDB;
                    thr = dynThreshEnv_;
                }
                else
                {
                    thr = dynThresholdDB_;
                }
                float over = lvlDB - thr;
                O = dynamicGainOffsetDB(over, dynRatio_, dynKneeDB_, rngAbs, rngSign);
            }
            lastDynOffsetDB_ = O;

            // Effective gain (linear) = static gain * 10^(O/20).
            float effLin = gLin * std::pow(10.f, O * 0.05f);
            float ff = jlimit(20.f, (float)(sampleRate_ * 0.499), f);

            IIRCoefficients c;
            switch (iirSubType_)
            {
                case IIRSubType::LowShelf:  c = IIRCoefficients::makeLowShelf  (sampleRate_, ff, q, effLin); break;
                case IIRSubType::HighShelf: c = IIRCoefficients::makeHighShelf (sampleRate_, ff, q, effLin); break;
                default:                    c = IIRCoefficients::makePeakFilter(sampleRate_, ff, q, effLin); break;
            }
            dynTarget_.setFromStandard(c.coefficients[0], c.coefficients[1], c.coefficients[2],
                                        c.coefficients[3], c.coefficients[4]);

            // Track the detector filter to the (possibly dragged) freq/Q. The
            // filter shape follows the band type (band-pass / low-pass / high-pass).
            if (!linked)
            {
                auto dc = makeDetectorCoeffs(iirSubType_, sampleRate_, ff, q);
                dynDetector_.setFromStandard(dc.coefficients[0], dc.coefficients[1], dc.coefficients[2],
                                              dc.coefficients[3], dc.coefficients[4]);
            }

            if (dynNeedsSnap_)
            {
                // Snap working coeffs to target (preserve filter state) — avoids
                // ramping from stale coefficients on the first block / after toggle.
                float v1 = iirWork_.v1, v2 = iirWork_.v2;
                iirWork_ = dynTarget_;
                iirWork_.v1 = v1;
                iirWork_.v2 = v2;
                dynRampLeft_ = 0;
                dynNeedsSnap_ = false;
            }
            else
            {
                dynRampLeft_ = kDynCtrlSamples;
            }
        }
        --dynCtrlCounter_;

        // --- Per-sample coefficient interpolation toward target ---
        if (dynRampLeft_ > 0)
        {
            float t = 1.f / (float)dynRampLeft_;
            iirWork_.b0 += (dynTarget_.b0 - iirWork_.b0) * t;
            iirWork_.a1 += (dynTarget_.a1 - iirWork_.a1) * t;
            iirWork_.a2 += (dynTarget_.a2 - iirWork_.a2) * t;
            iirWork_.c1 += (dynTarget_.c1 - iirWork_.c1) * t;
            iirWork_.c2 += (dynTarget_.c2 - iirWork_.c2) * t;
            --dynRampLeft_;
        }

        // --- Lookahead: process the main signal delayed by dynLookaheadSamples_,
        //     so the gain (from the undelayed localIn) anticipates it. ---
        float x = localIn;
        if (hasLA)
        {
            int rp = dynMainDelayPos_ - dynLookaheadSamples_;
            if (rp < 0) rp += dynLookaheadSamples_;
            x = dynMainDelayBuffer_[(size_t) rp];
            dynMainDelayBuffer_[(size_t) dynMainDelayPos_] = localIn;
            dynMainDelayPos_ = (dynMainDelayPos_ + 1) % dynLookaheadSamples_;
        }

        data[i] = iirWork_.process(x);
    }
}

void EqBand::applyDynamicGain(float* data, int numSamples, const float* chainInput)
{
    // Broadband compressor: the detector measures the full-band level (no
    // sidechain filter) and modulates the band's flat gain by the same offset
    // O(dB) mapping (threshold / ratio / knee / range) used by the dynamic EQ.
    // Linked bands consume externalDynOffset_ (one shared gain change on every
    // channel — an imaging-safe multichannel compressor).
    ignoreUnused(chainInput);
    const bool linked = (externalDynOffset_ != nullptr);
    const bool hasLA  = dynLookaheadSamples_ > 0 && ! dynMainDelayBuffer_.empty();

    const float rngAbs  = std::abs(dynRangeDB_);
    const float rngSign = (dynRangeDB_ < 0.f) ? -1.f : 1.f;
    const float autoCoeff = (float)std::exp(-(double)kDynCtrlSamples
                                            / (1.5 * jmax(1.0, sampleRate_)));

    for (int i = 0; i < numSamples; ++i)
    {
        float gLin    = smoothGainLin_.getNextValue();
        float localIn = data[i];

        // --- Broadband self-detection (independent mode) ---
        if (!linked)
        {
            float p = localIn * localIn;                       // no sidechain filter
            float c = (p > dynEnv_) ? dynAtkCoeff_ : dynRelCoeff_;
            dynEnv_ = c * dynEnv_ + (1.f - c) * p;
        }

        // --- Control-rate offset + target gain ---
        if (dynCtrlCounter_ <= 0)
        {
            dynCtrlCounter_ = kDynCtrlSamples;

            float O;
            if (linked)
                O = externalDynOffset_[i];
            else
            {
                float lvlDB = 10.f * std::log10(dynEnv_ + 1e-12f);
                float thr;
                if (dynAuto_)
                {
                    dynThreshEnv_ = autoCoeff * dynThreshEnv_ + (1.f - autoCoeff) * lvlDB;
                    thr = dynThreshEnv_;
                }
                else
                {
                    thr = dynThresholdDB_;
                }
                float over = lvlDB - thr;
                O = dynamicGainOffsetDB(over, dynRatio_, dynKneeDB_, rngAbs, rngSign);
            }
            lastDynOffsetDB_ = O;

            dynGainTarget_ = gLin * std::pow(10.f, O * 0.05f);
            if (dynNeedsSnap_) { dynGainWork_ = dynGainTarget_; dynGainRampLeft_ = 0; dynNeedsSnap_ = false; }
            else                 dynGainRampLeft_ = kDynCtrlSamples;
        }
        --dynCtrlCounter_;

        // --- Per-sample gain ramp toward target (zipper-free) ---
        if (dynGainRampLeft_ > 0)
        {
            dynGainWork_ += (dynGainTarget_ - dynGainWork_) / (float)dynGainRampLeft_;
            --dynGainRampLeft_;
        }

        // --- Lookahead: gain (from undelayed localIn) applied to delayed audio ---
        float x = localIn;
        if (hasLA)
        {
            int rp = dynMainDelayPos_ - dynLookaheadSamples_;
            if (rp < 0) rp += dynLookaheadSamples_;
            x = dynMainDelayBuffer_[(size_t) rp];
            dynMainDelayBuffer_[(size_t) dynMainDelayPos_] = localIn;
            dynMainDelayPos_ = (dynMainDelayPos_ + 1) % dynLookaheadSamples_;
        }

        data[i] = x * dynGainWork_;
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

    // FFT convolution typically beats direct form above ~64-128 taps,
    // regardless of block size (with vDSP / FFTW hardware-accelerated FFT).
    static constexpr int kConvolverThreshold = 128;

    if (firCoeffs_.empty() || maxBlockSize_ <= 0)
        return;

    int firLen = (int)firCoeffs_.size();

    // Symmetric (linear-phase) FIRs have an intrinsic group delay of (N-1)/2
    // samples — the impulse-response peak sits at the centre of the kernel, so
    // the host must compensate for that delay via PDC. Asymmetric FIRs have no
    // single well-defined "latency" (the energy isn't concentrated at one tap),
    // so we report 0 group delay for them — matching the pre-existing behaviour.
    bool symmetric = true;
    {
        constexpr float tol = 1e-6f;
        for (int i = 0; i < firLen / 2; ++i)
        {
            if (std::abs (firCoeffs_[(size_t) i] - firCoeffs_[(size_t) (firLen - 1 - i)]) > tol)
            {
                symmetric = false;
                break;
            }
        }
    }
    int groupDelay = symmetric ? (firLen - 1) / 2 : 0;

    if (firLen <= kConvolverThreshold)
    {
        // Short FIR — direct convolution. No convolver overhead, only the FIR's
        // own group delay (if symmetric).
        convolverLatency_ = groupDelay;
        return;
    }

    convolver_ = std::make_unique<MtxConvMaster>();
    int maxPart = jmax(8192, maxBlockSize_);
    if (!convolver_->Configure(1, 1, maxBlockSize_, firLen, maxBlockSize_, maxPart))
    {
        convolver_.reset();
        // Convolver setup failed; still report group delay since the audio
        // path won't run at all in that case.
        convolverLatency_ = groupDelay;
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
    convolverLatency_ = maxBlockSize_ + groupDelay;
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

std::complex<float> EqBand::getFrequencyResponse(double freqHz, bool alwaysCompute, float extraGainDB) const
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
            // For dynamic peak/shelf bands, recompute coefficients at the live
            // effective gain so the displayed curve tracks the dynamic action.
            IIRCoefficients dynC;
            const float* c = iirCoeffs_.coefficients;
            if (extraGainDB != 0.f && !hasRawCoeffs_
                && (iirSubType_ == IIRSubType::Peak
                    || iirSubType_ == IIRSubType::LowShelf
                    || iirSubType_ == IIRSubType::HighShelf))
            {
                float effLin = Decibels::decibelsToGain(gainDB_ + extraGainDB);
                float f = jlimit(20.f, (float)(sampleRate_ * 0.499), frequency_);
                switch (iirSubType_)
                {
                    case IIRSubType::LowShelf:  dynC = IIRCoefficients::makeLowShelf  (sampleRate_, f, q_, effLin); break;
                    case IIRSubType::HighShelf: dynC = IIRCoefficients::makeHighShelf (sampleRate_, f, q_, effLin); break;
                    default:                    dynC = IIRCoefficients::makePeakFilter(sampleRate_, f, q_, effLin); break;
                }
                c = dynC.coefficients;
            }
            float b0 = c[0], b1 = c[1], b2 = c[2], a1 = c[3], a2 = c[4];

            std::complex<double> num = (double)b0 + (double)b1 * z_inv + (double)b2 * z_inv2;
            std::complex<double> den = 1.0 + (double)a1 * z_inv + (double)a2 * z_inv2;

            auto resp = num / den;
            return std::complex<float>((float)resp.real(), (float)resp.imag());
        }

        case EqBandType::FIR:
        {
            if (firFFT_.empty() || firFFTSize_ <= 0)
                return std::complex<float>(1.f, 0.f);

            // Look up the precomputed FFT spectrum. We interpolate *magnitude*
            // between adjacent bins, not the complex (real,imag) pair: for long
            // symmetric (linear-phase) FIRs the phase rotates by ~π per bin
            // (group delay (N-1)/2), so linear interpolation of complex pairs
            // would give wildly wrong magnitudes — visible as a comb-filter
            // mess when you sweep frequency across bins.
            double binFreq = sampleRate_ / (double)firFFTSize_;
            double binIdx = freqHz / binFreq;
            int numBins = (int)firFFT_.size();

            if (binIdx <= 0.0)
                return std::complex<float>(std::abs(firFFT_[0]), 0.f);
            if (binIdx >= numBins - 1)
                return std::complex<float>(std::abs(firFFT_[numBins - 1]), 0.f);

            int i0 = (int)binIdx;
            float frac = (float)(binIdx - i0);
            float mag0 = std::abs(firFFT_[i0]);
            float mag1 = std::abs(firFFT_[i0 + 1]);
            float mag  = mag0 + frac * (mag1 - mag0);
            // Magnitude-only return (zero phase). Chain multiplication keeps
            // |∏ H_i| = ∏ |H_i| consistent for the displayed dB curve.
            return std::complex<float>(mag, 0.f);
        }

        case EqBandType::Gain:
        {
            // Broadband gain — flat across frequency. The live dynamic offset
            // (extraGainDB) shifts the whole line so the graph tracks compression.
            float g = linearGain_;
            if (extraGainDB != 0.f)
                g *= Decibels::decibelsToGain(extraGainDB);
            return std::complex<float>(g, 0.f);
        }

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
        case IIRSubType::Chebyshev1LP:  return "chebyshev1_lp";
        case IIRSubType::Chebyshev1HP:  return "chebyshev1_hp";
        case IIRSubType::Chebyshev2LP:  return "chebyshev2_lp";
        case IIRSubType::Chebyshev2HP:  return "chebyshev2_hp";
        case IIRSubType::EllipticLP:    return "elliptic_lp";
        case IIRSubType::EllipticHP:    return "elliptic_hp";
        case IIRSubType::BesselLP:      return "bessel_lp";
        case IIRSubType::BesselHP:      return "bessel_hp";
        case IIRSubType::Tilt:          return "tilt";
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
    if (s == "chebyshev1_lp") return IIRSubType::Chebyshev1LP;
    if (s == "chebyshev1_hp") return IIRSubType::Chebyshev1HP;
    if (s == "chebyshev2_lp") return IIRSubType::Chebyshev2LP;
    if (s == "chebyshev2_hp") return IIRSubType::Chebyshev2HP;
    if (s == "elliptic_lp")   return IIRSubType::EllipticLP;
    if (s == "elliptic_hp")   return IIRSubType::EllipticHP;
    if (s == "bessel_lp")     return IIRSubType::BesselLP;
    if (s == "bessel_hp")     return IIRSubType::BesselHP;
    if (s == "tilt")          return IIRSubType::Tilt;
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
                if (originalSampleRate_ > 0.0)
                    params->setProperty("sample_rate", originalSampleRate_);
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
                    else if (isAnalogPrototypeSubType(iirSubType_))
                    {
                        params->setProperty("order", analogOrder_);
                        bool isCheby1 = iirSubType_ == IIRSubType::Chebyshev1LP || iirSubType_ == IIRSubType::Chebyshev1HP;
                        bool isCheby2 = iirSubType_ == IIRSubType::Chebyshev2LP || iirSubType_ == IIRSubType::Chebyshev2HP;
                        bool isElliptic = iirSubType_ == IIRSubType::EllipticLP || iirSubType_ == IIRSubType::EllipticHP;
                        if (isCheby1 || isElliptic)
                            params->setProperty("ripple_pass_db", ripplePassDB_);
                        if (isCheby2 || isElliptic)
                            params->setProperty("ripple_stop_db", rippleStopDB_);
                    }
                    else if (iirSubType_ == IIRSubType::Tilt)
                    {
                        // Tilt has no Q; its "gain" is a slope, so name it as one.
                        params->setProperty("slope_db_oct", gainDB_);
                        params->setProperty("tilt_lo_hz", tiltLoHz_);
                        params->setProperty("tilt_hi_hz", tiltHiHz_);
                    }
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
                if (originalSampleRate_ > 0.0)
                    params->setProperty("sample_rate", originalSampleRate_);
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
                if (designerState_.isObject())
                    params->setProperty("designer", designerState_);
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

        // Dynamic EQ — only emitted when active, so existing presets are unchanged.
        if (dynActive_ && supportsDynamic())
        {
            auto* dyn = new DynamicObject();
            dyn->setProperty("active", true);
            dyn->setProperty("threshold_db", dynThresholdDB_);
            dyn->setProperty("range_db", dynRangeDB_);
            dyn->setProperty("ratio", dynRatio_);
            dyn->setProperty("knee_db", dynKneeDB_);
            dyn->setProperty("attack_ms", dynAttackMs_);
            dyn->setProperty("release_ms", dynReleaseMs_);
            if (dynAuto_)
                dyn->setProperty("auto", true);
            dyn->setProperty("linked", dynLinked_);
            if (dynLookaheadMs_ > 0.f)
                dyn->setProperty("lookahead_ms", dynLookaheadMs_);
            params->setProperty("dynamic", var(dyn));
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

            if (params.hasProperty("designer"))
                band->setDesignerState(params["designer"]);
        }
        else
        {
            // IIR parameterized
            band->setType(EqBandType::IIR);
            double iirSR = (double)params.getProperty("sample_rate", 0.0);
            if (iirSR > 0.0)
                band->originalSampleRate_ = iirSR;
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
                else if (isAnalogPrototypeSubType(subType))
                {
                    bool isCheby1 = subType == IIRSubType::Chebyshev1LP || subType == IIRSubType::Chebyshev1HP;
                    bool isCheby2 = subType == IIRSubType::Chebyshev2LP || subType == IIRSubType::Chebyshev2HP;
                    bool isElliptic = subType == IIRSubType::EllipticLP || subType == IIRSubType::EllipticHP;
                    if (isCheby1 || isElliptic)
                        band->setRipplePassDB((float)params.getProperty("ripple_pass_db", 1.0));
                    if (isCheby2 || isElliptic)
                        band->setRippleStopDB((float)params.getProperty("ripple_stop_db", 60.0));
                    // setAnalogOrder triggers the coefficient rebuild — call it last so
                    // ripple values are in place when coefficients are computed.
                    band->setAnalogOrder((int)params.getProperty("order", 4));
                }
                else if (subType == IIRSubType::Tilt)
                {
                    // Slope in dB/octave; "gain_db" accepted as a fallback spelling.
                    band->setTiltLoHz((float)params.getProperty("tilt_lo_hz",
                                                                kTiltDefaultLoHz));
                    band->setTiltHiHz((float)params.getProperty("tilt_hi_hz",
                                                                kTiltDefaultHiHz));
                    band->setGainDB((float)params.getProperty(
                        "slope_db_oct", params.getProperty("gain_db", 0.0)));
                }
                else
                {
                    band->setQ((float)params.getProperty("Q", 0.707));
                    band->setGainDB((float)params.getProperty("gain_db", 0.0));
                }
            }
        }

        // Dynamic parameters — applies to any band that supportsDynamic():
        // peak/shelf IIR, or a broadband Gain band (multichannel compressor).
        // Parsed at the shared parameters scope so the Gain branch above also
        // picks it up; supportsDynamic() gates whether it does anything.
        var dyn = params["dynamic"];
        if (dyn.isObject())
        {
            band->setDynThresholdDB((float)dyn.getProperty("threshold_db", -24.0));
            band->setDynRangeDB((float)dyn.getProperty("range_db", -6.0));
            band->setDynRatio((float)dyn.getProperty("ratio", 4.0));
            band->setDynKneeDB((float)dyn.getProperty("knee_db", 6.0));
            band->setDynAttackMs((float)dyn.getProperty("attack_ms", 10.0));
            band->setDynReleaseMs((float)dyn.getProperty("release_ms", 120.0));
            band->setDynAuto((bool)dyn.getProperty("auto", false));
            band->setDynLinked((bool)dyn.getProperty("linked", true));
            band->setDynLookaheadMs((float)dyn.getProperty("lookahead_ms", 0.0));
            band->setDynamicActive((bool)dyn.getProperty("active", true));
        }
    }

    if (json.hasProperty("enabled"))
        band->setEnabled((bool)json["enabled"]);

    return band;
}
