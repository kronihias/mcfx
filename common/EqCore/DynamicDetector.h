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

#ifndef DYNAMICDETECTOR_H_INCLUDED
#define DYNAMICDETECTOR_H_INCLUDED

#include "EqBand.h"   // BiquadSection, IIRCoefficients (via JuceHeader)
#include <set>
#include <vector>

//==============================================================================
/** Shared cross-channel level detector for a *linked* dynamic EQ band.

    Runs one band-pass (at the band's freq/Q) on each active channel of the chain
    input, sums their power into a single shared envelope, and produces a per-sample
    gain-offset (dB) trajectory that every channel copy of the band then applies
    identically — so a linked band changes all channels by the same amount,
    preserving Ambisonic / stereo imaging.

    The offset computation mirrors EqBand::applyDynamicIIR exactly (same envelope,
    threshold/auto, range mapping, control-rate cadence) so linked and independent
    modes behave consistently. */
class DynamicDetector
{
public:
    DynamicDetector() = default;

    void prepare(double sampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        numChannels_ = jmax(0, numChannels);
        bandpasses_.assign((size_t) numChannels_, BiquadSection{});
        offsetDB_.assign((size_t) jmax(1, maxBlockSize), 0.f);
        env_ = 0.f;
        threshEnv_ = thresholdDB_;
        ctrlCounter_ = 0;
        currentOffsetDB_ = 0.f;
        updateEnvCoeffs();
        updateDetectorCoeffs();
        setDetectionDelaySamples(detDelaySamples_);   // (re)allocate per-channel delay lines
        for (auto& bp : bandpasses_)
            bp.resetState();
    }

    /** Per-channel input delay (samples) applied before the detector filter, so a
        linked band's offset aligns with its local (post-upstream) signal. */
    void setDetectionDelaySamples(int n)
    {
        detDelaySamples_ = jmax(0, n);
        detDelayPos_.assign((size_t) numChannels_, 0);
        detDelayLines_.assign((size_t) numChannels_,
                              std::vector<float>((size_t) detDelaySamples_, 0.f));
    }

    void setParams(IIRSubType subType, float freq, float Q, float thresholdDB, float rangeDB,
                   float attackMs, float releaseMs, bool autoThresh)
    {
        subType_ = subType;
        freq_ = freq; q_ = Q;
        thresholdDB_ = thresholdDB; rangeDB_ = rangeDB;
        attackMs_ = jmax(0.1f, attackMs); releaseMs_ = jmax(1.0f, releaseMs);
        auto_ = autoThresh;
        updateEnvCoeffs();
        updateDetectorCoeffs();
    }

    void setBandIndex(int idx) { bandIndex_ = idx; }
    int  getBandIndex() const  { return bandIndex_; }

    /** Fill offsetDB_ for this block from the summed/mean power of the active
        channels (1-based; empty set => all channels). */
    void computeOffsets(const AudioBuffer<float>& input,
                        const std::set<int>& activeCh1based, int numSamples)
    {
        if ((int) offsetDB_.size() < numSamples)
            offsetDB_.assign((size_t) numSamples, 0.f);

        const int numCh = jmin(input.getNumChannels(), (int) bandpasses_.size());

        // Pre-resolve the participating channel indices once per block.
        activeIdx_.clear();
        for (int ch = 0; ch < numCh; ++ch)
            if (activeCh1based.empty() || activeCh1based.count(ch + 1) > 0)
                activeIdx_.push_back(ch);

        if (activeIdx_.empty())
        {
            std::fill(offsetDB_.begin(), offsetDB_.begin() + numSamples, 0.f);
            currentOffsetDB_ = 0.f;
            return;
        }

        const float rngAbs  = std::abs(rangeDB_);
        const float rngSign = (rangeDB_ < 0.f) ? -1.f : 1.f;
        const float autoCoeff = (float) std::exp(-(double) kCtrl / (1.5 * jmax(1.0, sampleRate_)));

        float lastO = currentOffsetDB_;

        for (int i = 0; i < numSamples; ++i)
        {
            // Reduction = max power across active channels: channel-count-robust (a
            // few fed channels of a high-channel instance still trigger reliably)
            // and still drives one shared gain on every channel (imaging preserved).
            float p = 0.f;
            for (int ch : activeIdx_)
            {
                float x = input.getReadPointer(ch)[i];
                if (detDelaySamples_ > 0)
                {
                    auto& line = detDelayLines_[(size_t) ch];
                    int rp = detDelayPos_[(size_t) ch] - detDelaySamples_;
                    if (rp < 0) rp += detDelaySamples_;
                    float d = line[(size_t) rp];
                    line[(size_t) detDelayPos_[(size_t) ch]] = x;
                    detDelayPos_[(size_t) ch] = (detDelayPos_[(size_t) ch] + 1) % detDelaySamples_;
                    x = d;
                }
                float bp = bandpasses_[(size_t) ch].process(x);
                float pc = bp * bp;
                if (pc > p) p = pc;
            }

            float c = (p > env_) ? atkCoeff_ : relCoeff_;
            env_ = c * env_ + (1.f - c) * p;

            if (ctrlCounter_ <= 0)
            {
                ctrlCounter_ = kCtrl;
                float lvlDB = 10.f * std::log10(env_ + 1e-12f);
                float thr;
                if (auto_)
                {
                    threshEnv_ = autoCoeff * threshEnv_ + (1.f - autoCoeff) * lvlDB;
                    thr = threshEnv_;
                }
                else
                {
                    thr = thresholdDB_;
                }
                float over = lvlDB - thr;
                lastO = jlimit(0.f, rngAbs, over) * rngSign;
            }
            --ctrlCounter_;
            offsetDB_[(size_t) i] = lastO;
        }

        currentOffsetDB_ = lastO;
    }

    const float* offsets() const { return offsetDB_.data(); }
    float current() const { return currentOffsetDB_; }

private:
    void updateEnvCoeffs()
    {
        double atkSamples = jmax(1.0, (double) attackMs_  * 0.001 * sampleRate_);
        double relSamples = jmax(1.0, (double) releaseMs_ * 0.001 * sampleRate_);
        atkCoeff_ = (float) std::exp(-1.0 / atkSamples);
        relCoeff_ = (float) std::exp(-1.0 / relSamples);
    }

    void updateDetectorCoeffs()
    {
        // Detector shape follows the band type (band-pass / low-pass / high-pass).
        auto dc = EqBand::makeDetectorCoeffs(subType_, sampleRate_, freq_, q_);
        for (auto& bp : bandpasses_)
            bp.setFromStandard(dc.coefficients[0], dc.coefficients[1], dc.coefficients[2],
                               dc.coefficients[3], dc.coefficients[4]);   // preserves state
    }

    static constexpr int kCtrl = 32;   // must match EqBand::kDynCtrlSamples

    double sampleRate_ = 48000.0;
    int    bandIndex_  = -1;
    int    numChannels_ = 0;

    // Detection input delay (D_before) — per-channel ring buffers.
    int detDelaySamples_ = 0;
    std::vector<std::vector<float>> detDelayLines_;
    std::vector<int> detDelayPos_;

    IIRSubType subType_ = IIRSubType::Peak;
    float freq_ = 1000.f, q_ = 0.707f;
    float thresholdDB_ = -24.f, rangeDB_ = -6.f;
    float attackMs_ = 10.f, releaseMs_ = 120.f;
    bool  auto_ = false;

    std::vector<BiquadSection> bandpasses_;   // one per channel
    std::vector<int>           activeIdx_;     // scratch: participating channel indices
    std::vector<float>         offsetDB_;       // per-sample offset trajectory

    float env_ = 0.f;
    float threshEnv_ = -60.f;
    float atkCoeff_ = 0.f, relCoeff_ = 0.f;
    int   ctrlCounter_ = 0;
    float currentOffsetDB_ = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicDetector)
};

#endif // DYNAMICDETECTOR_H_INCLUDED
