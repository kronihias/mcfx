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

#ifndef EQBAND_H_INCLUDED
#define EQBAND_H_INCLUDED

#include "JuceHeader.h"
#include "MtxConv.h"
#include <array>
#include <complex>
#include <memory>
#include <vector>

//==============================================================================
/** Dynamic-EQ gain mapping (shared by EqBand::applyDynamicIIR and the linked
    DynamicDetector so both behave identically).

    Given `over` = detector level minus threshold (dB), returns the signed gain
    offset (dB) whose magnitude is capped at `rngAbs` and whose sign is `rngSign`
    (< 0 compress, > 0 boost):

      slope  = 1 - 1/ratio        (ratio >= 1; 1 = no action, large = hard/limit)
      knee   = soft-knee width dB (0 = hard corner at threshold)
      g      = 0                       for over <= -knee/2
               slope * over            for over >=  knee/2
               slope * (over+knee/2)^2 / (2*knee)   in the knee
      offset = clamp(g, 0, rngAbs) * rngSign
*/
inline float dynamicGainOffsetDB(float over, float ratio, float kneeDB,
                                 float rngAbs, float rngSign) noexcept
{
    const float slope = 1.0f - 1.0f / juce::jmax(1.0f, ratio);
    const float hk = 0.5f * kneeDB;
    float g;
    if (over <= -hk)          g = 0.0f;
    else if (over >= hk)      g = slope * over;
    else                                              // quadratic soft knee
    {
        const float t = over + hk;
        g = slope * (t * t) / (2.0f * kneeDB);
    }
    return juce::jlimit(0.0f, rngAbs, g) * rngSign;
}

//==============================================================================
/** Modified Transposed Direct-Form II biquad section.
    Pre-computes c1 = b1 - a1*b0, c2 = b2 - a2*b0 to reduce operations
    in the state update (same form as SmoothIIRFilter). */
struct BiquadSection
{
    float b0 = 1.f, a1 = 0.f, a2 = 0.f, c1 = 0.f, c2 = 0.f;
    float v1 = 0.f, v2 = 0.f;

    void setFromStandard(float sb0, float sb1, float sb2, float sa1, float sa2)
    {
        b0 = sb0; a1 = sa1; a2 = sa2;
        c1 = sb1 - sa1 * sb0;
        c2 = sb2 - sa2 * sb0;
    }

    inline float process(float x) noexcept
    {
        float y  = b0 * x + v1;
        float t1 = c1 * x + v2;
        float t0 = -a2 * v1;
        v1 = -a1 * v1 + t1;
        v2 = c2 * x + t0;
        return y;
    }

    void resetState() { v1 = 0.f; v2 = 0.f; }
};

enum class EqBandType
{
    IIR,
    FIR,
    Gain,
    Delay
};

enum class IIRSubType
{
    LowPass,
    HighPass,
    BandPass,
    Notch,
    AllPass,
    LowShelf,
    HighShelf,
    Peak,
    ButterworthLP,
    ButterworthHP,
    CrossoverLP,
    CrossoverHP,
    CrossoverAP,
    RawBiquad,
    Chebyshev1LP,
    Chebyshev1HP,
    Chebyshev2LP,
    Chebyshev2HP,
    EllipticLP,
    EllipticHP,
    BesselLP,
    BesselHP,
    Tilt
};

class EqBand
{
public:
    EqBand();
    ~EqBand();

    // --- Type ---
    EqBandType getType() const { return type_; }
    void setType(EqBandType t);

    // --- IIR parameters ---
    IIRSubType getIIRSubType() const { return iirSubType_; }
    void setIIRSubType(IIRSubType st);

    float getFrequency() const { return frequency_; }
    void setFrequency(float f);

    float getQ() const { return q_; }
    void setQ(float q);

    float getGainDB() const { return gainDB_; }
    void setGainDB(float db);

    int getButterworthOrder() const { return butterworthOrder_; }
    void setButterworthOrder(int order);

    int getCrossoverOrder() const { return crossoverOrder_; }
    void setCrossoverOrder(int lrOrder);

    // --- Higher-order IIR family parameters (Chebyshev I/II, Bessel) ---
    int getAnalogOrder() const { return analogOrder_; }
    void setAnalogOrder(int order);

    float getRipplePassDB() const { return ripplePassDB_; }
    void setRipplePassDB(float db);

    float getRippleStopDB() const { return rippleStopDB_; }
    void setRippleStopDB(float db);

    // --- FIR ---
    const std::vector<float>& getFIRCoefficients() const { return firCoeffs_; }
    void setFIRCoefficients(const std::vector<float>& coeffs);
    void setFIRCoefficientsWithSampleRate(const std::vector<float>& coeffs, double sampleRate);
    bool loadFIRFromFile(const File& file, int channel = 0);

    const std::vector<float>& getFIROriginalCoefficients() const { return firOriginalCoeffs_; }
    double getFIRSampleRate() const { return originalSampleRate_; }
    void setFIRSampleRate(double sr) { originalSampleRate_ = sr; }
    const String& getFIRFilePath() const { return firFilePath_; }
    void setFIRFilePath(const String& path) { firFilePath_ = path; }
    int getFIRFileChannel() const { return firFileChannel_; }
    void setFIRFileChannel(int ch) { firFileChannel_ = ch; }

    // --- Designer state (opaque var blob from FIRDesignDialog) ---
    // When the FIR was generated by the linear-phase FIR designer dialog, the
    // dialog's parameter values are stored here and round-tripped through the
    // band's JSON, so re-opening the designer picks up where the user left off.
    const var& getDesignerState() const { return designerState_; }
    void setDesignerState(const var& s) { designerState_ = s; }

    // General original sample rate (used for both IIR raw biquad resampling and FIR resampling)
    double getOriginalSampleRate() const { return originalSampleRate_; }
    void setOriginalSampleRate(double sr);

    /** Returns the latency introduced by the partitioned convolver (0 if not active). */
    int getConvolverLatency() const { return convolverLatency_; }

    // --- Raw IIR coefficients (direct biquad) ---
    struct BiquadCoeffs { float b0, b1, b2, a0, a1, a2; };
    bool hasRawCoefficients() const { return hasRawCoeffs_; }
    BiquadCoeffs getRawCoefficients() const { return rawCoeffs_; }
    void setRawCoefficients(float b0, float b1, float b2, float a0, float a1, float a2);

    /** Check if biquad denominator coefficients are stable (poles inside unit circle).
        Expects normalized form (a0 = 1). */
    static bool isBiquadStable(float a1, float a2);

    /** Returns true if current raw coefficients are stable (or if not in raw mode). */
    bool isRawCoeffsStable() const;

    // --- Gain band mode ---
    bool getUseLinearGain() const { return useLinearGain_; }
    void setUseLinearGain(bool linear);

    float getLinearGain() const { return linearGain_; }
    void setLinearGain(float g);

    bool getInvertGain() const { return invertGain_; }
    void setInvertGain(bool inv);

    // --- Delay ---
    int getDelaySamples() const { return delaySamples_; }
    void setDelaySamples(int samples);

    // --- Sample rate (read-only, set by prepare()) ---
    double getSampleRate() const { return sampleRate_; }

    // --- Enable ---
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    // --- Dynamic EQ (per-band dynamics) ---
    // Only meaningful for gain-bearing single-biquad IIR types (Peak / Low/High shelf).
    bool supportsDynamic() const;
    /** True for band types whose dynamic detector is broadband (full-band level)
        rather than a frequency-selective sidechain — i.e. the Gain compressor. */
    bool isDynamicBroadband() const;

    /** Sidechain/detector filter for a dynamic band — chosen to match the region
        the filter actually affects: band-pass for Peak, low-pass for Low Shelf,
        high-pass for High Shelf. Tracks the band's frequency and Q. */
    static IIRCoefficients makeDetectorCoeffs(IIRSubType subType, double sampleRate,
                                              float freq, float q);

    /** Tilt ("spectral seesaw"): lifts one half of the spectrum by +gain and drops
        the other by -gain, pivoting through 0 dB at `freq`. Built as a low shelf of
        twice the gain, scaled back by half of it — so a single biquad gives
        +gain at DC, -gain at Nyquist, and unity at the pivot. Positive gain tilts
        the balance towards the lows (darker), negative towards the highs
        (brighter). `gainLinear` is linear, matching the JUCE make*() convention. */
    static IIRCoefficients makeTiltCoeffs(double sampleRate, float freq, float q,
                                          float gainLinear);

    bool  isDynamicActive() const { return dynActive_; }
    void  setDynamicActive(bool b);

    float getDynThresholdDB() const { return dynThresholdDB_; }
    void  setDynThresholdDB(float db) { dynThresholdDB_ = db; }

    float getDynRangeDB() const { return dynRangeDB_; }       // signed: <0 = compress, >0 = expand/boost
    void  setDynRangeDB(float db) { dynRangeDB_ = db; }

    float getDynRatio() const { return dynRatio_; }           // N:1 (slope = 1 - 1/ratio)
    void  setDynRatio(float r) { dynRatio_ = jmax(1.0f, r); }

    float getDynKneeDB() const { return dynKneeDB_; }         // soft-knee width (dB); 0 = hard
    void  setDynKneeDB(float db) { dynKneeDB_ = jmax(0.0f, db); }

    float getDynAttackMs() const { return dynAttackMs_; }
    void  setDynAttackMs(float ms) { dynAttackMs_ = jmax(0.1f, ms); updateDynEnvCoeffs(); }

    float getDynReleaseMs() const { return dynReleaseMs_; }
    void  setDynReleaseMs(float ms) { dynReleaseMs_ = jmax(1.0f, ms); updateDynEnvCoeffs(); }

    bool  getDynAuto() const { return dynAuto_; }
    void  setDynAuto(bool b) { dynAuto_ = b; }

    bool  getDynLinked() const { return dynLinked_; }
    void  setDynLinked(bool b) { dynLinked_ = b; }

    float getDynLookaheadMs() const { return dynLookaheadMs_; }
    void  setDynLookaheadMs(float ms) { dynLookaheadMs_ = jlimit(0.f, 50.f, ms); }
    /** Lookahead in samples (0 unless this is an active dynamic band). Used by the
        chain/processor for latency accounting and to size the main delay. */
    int   getLookaheadSamples(double sampleRate) const;

    /** Linked mode: the processor supplies a per-sample dB-offset buffer for this block
        (computed once across all linked channels). Pass nullptr to self-detect. */
    void  setExternalDynOffset(const float* buf) { externalDynOffset_ = buf; }

    /** This (processing-copy) band's last computed dynamic offset in dB. Read on the
        audio thread by the processor to publish GUI metering (no cross-thread access). */
    float getLastDynOffsetDB() const { return lastDynOffsetDB_; }

    // --- Channel routing (for mimo_eq) ---
    int getInputChannel() const { return inputChannel_; }
    void setInputChannel(int ch) { inputChannel_ = ch; }
    int getOutputChannel() const { return outputChannel_; }
    void setOutputChannel(int ch) { outputChannel_ = ch; }
    bool isDiagonal() const { return diagonal_; }
    void setDiagonal(bool d) { diagonal_ = d; }

    // --- Processing ---
    void prepare(double sampleRate, int maxBlockSize);
    void processBlock(float* data, int numSamples);
    /** Dynamic-aware processing. chainInput points to the unprocessed chain-input
        signal used for self-detection (independent mode); ignored when an external
        offset has been set (linked mode). For non-dynamic bands this is equivalent
        to processBlock(data, numSamples). */
    void processBlock(float* data, int numSamples, const float* chainInput);
    void reset();

    /** Copy parameters from source without resetting filter state.
        Triggers smooth coefficient transition for click-free updates. */
    void syncParametersFrom(const EqBand& source);

    // --- Frequency response ---
    // Returns complex response at frequency f (Hz) for magnitude+phase
    // If alwaysCompute is true, returns the response even when disabled.
    // extraGainDB adds a live gain offset (peak/shelf only) so the displayed
    // curve can reflect the current dynamic gain.
    std::complex<float> getFrequencyResponse(double freqHz, bool alwaysCompute = false,
                                             float extraGainDB = 0.f) const;

    /** Rebuild the cached FIR frequency response via FFT.
        Called automatically when FIR coefficients change. */
    void rebuildFIRFrequencyResponse();

    // --- JSON ---
    var toJson() const;
    static EqBand* fromJson(const var& json);

    bool usesCascade() const;

private:
    void updateIIRCoefficients();
    void applyIIR(float* data, int numSamples);
    void applyDynamicIIR(float* data, int numSamples, const float* chainInput);
    void applyDynamicGain(float* data, int numSamples, const float* chainInput);
    void updateDynEnvCoeffs();   // recompute attack/release one-pole coeffs from ms + SR
    void applyCascadeIIR(float* data, int numSamples);
    void applyFIR(float* data, int numSamples);
    void resampleFIRCoefficients();
    void resampleRawBiquad();
    void rebuildConvolver();
    void applyGain(float* data, int numSamples);
    void applyDelay(float* data, int numSamples);
    void startSmoothing();       // set up smoothing after updateIIRCoefficients
    void recalcWorkingCoeffs();  // per-sample coeff recalc from smoothed params (single biquad)

    EqBandType type_ = EqBandType::IIR;
    bool enabled_ = true;

    // IIR
    IIRSubType iirSubType_ = IIRSubType::Peak;
    float frequency_ = 1000.f;
    float q_ = 0.707f;
    float gainDB_ = 0.f;
    bool hasRawCoeffs_ = false;
    BiquadCoeffs rawCoeffs_ = { 1.f, 0.f, 0.f, 1.f, 0.f, 0.f };
    int butterworthOrder_ = 2;
    int crossoverOrder_ = 4;  // LR order: 2, 4, 6, 8

    // Chebyshev / Bessel cascade parameters
    int   analogOrder_   = 4;
    float ripplePassDB_  = 1.0f;     // Chebyshev I passband ripple
    float rippleStopDB_  = 60.0f;    // Chebyshev II stopband attenuation (positive dB)
    std::vector<std::array<float, 5>> cascadeCoeffs_;  // [b0,b1,b2,a1,a2] per section (for freq response)

    // Standard-form coefficients (for frequency response display)
    IIRCoefficients iirCoeffs_;
    double sampleRate_ = 48000.0;

    // Modified TDF-II working state (for processing)
    BiquadSection iirWork_;                        // single biquad: current working coeffs + state
    std::vector<BiquadSection> cascadeWork_;        // cascade: current working sections + state
    std::vector<BiquadSection> cascadeTarget_;      // cascade: target coefficients

    // Parameter smoothing for single biquad types (same approach as SmoothIIRFilter)
    SmoothedValue<float, ValueSmoothingTypes::Linear> smoothFreq_;
    SmoothedValue<float, ValueSmoothingTypes::Linear> smoothQ_;
    SmoothedValue<float, ValueSmoothingTypes::Linear> smoothGainLin_; // linear gain

    // Coefficient smoothing for cascade types (parameter recomputation too expensive)
    int smoothSamplesLeft_ = 0;
    static constexpr int kSmoothRampSamples = 128;  // ~2.7ms at 48kHz

    bool prepared_ = false;
    int maxBlockSize_ = 0;

    // FIR
    std::vector<float> firCoeffs_;           // working coefficients (possibly resampled)
    std::vector<float> firState_;            // circular buffer for FIR convolution
    std::vector<float> firOriginalCoeffs_;   // original coefficients before resampling
    double originalSampleRate_ = 0.0;     // sample rate of original coefficients (0 = unknown/same as processing)
    String firFilePath_;                     // path to source WAV/text file (empty if not from file)
    int firFileChannel_ = 0;                 // which channel from WAV file
    var designerState_;                      // designer-dialog parameters (round-tripped via JSON)

    // Gain
    float linearGain_ = 1.f;
    bool useLinearGain_ = false;  // false = dB mode (default), true = linear mode
    bool invertGain_ = false;     // polarity invert (dB mode only)

    // Delay
    int delaySamples_ = 0;
    std::vector<float> delayBuffer_;
    int delayWritePos_ = 0;

    // Channel routing
    int inputChannel_ = -1;  // -1 = not set (diagonal)
    int outputChannel_ = -1;
    bool diagonal_ = true;

    // --- Dynamic EQ ---
    // Model parameters (default inert):
    bool  dynActive_      = false;
    float dynThresholdDB_ = -24.f;
    float dynRangeDB_     = -6.f;    // signed
    float dynRatio_       = 4.f;     // N:1
    float dynKneeDB_      = 6.f;     // soft-knee width (dB)
    float dynAttackMs_    = 10.f;
    float dynReleaseMs_   = 120.f;
    bool  dynAuto_        = false;
    bool  dynLinked_      = true;
    float dynLookaheadMs_ = 0.f;

    // Processing state (per processing copy):
    BiquadSection dynDetector_;          // bandpass at freq/Q used for level detection
    int   dynLookaheadSamples_ = 0;      // main-delay length (lookahead); 0 = none
    float dynGainTarget_ = 1.f;          // Gain-band compressor: target linear gain
    float dynGainWork_   = 1.f;          // ...current ramped linear gain
    int   dynGainRampLeft_ = 0;          // ...samples left in the control-rate ramp
    std::vector<float> dynMainDelayBuffer_; // ring buffer for the lookahead main delay
    int   dynMainDelayPos_ = 0;
    float dynEnv_       = 0.f;           // smoothed detection power
    float dynAtkCoeff_  = 0.f;           // one-pole attack coeff
    float dynRelCoeff_  = 0.f;           // one-pole release coeff
    float dynThreshEnv_ = -60.f;         // slow level average for auto-threshold (dB)
    BiquadSection dynTarget_;            // target working coeffs at control rate
    int   dynCtrlCounter_ = 0;           // samples until next control-rate recompute
    int   dynRampLeft_    = 0;           // samples left in coeff interpolation
    bool  dynNeedsSnap_   = true;        // snap (don't ramp) coeffs on first/after activation
    float lastDynOffsetDB_ = 0.f;        // last computed offset (audio thread, for processor)
    const float* externalDynOffset_ = nullptr;   // linked: per-sample dB offsets (set per block)
    static constexpr int kDynCtrlSamples = 32;   // control-rate period for coeff recompute

    // Cached FIR frequency response (computed via FFT for fast display)
    std::vector<std::complex<float>> firFFT_;  // half-spectrum (N/2+1 bins)
    int firFFTSize_ = 0;                        // FFT size used

    // Partitioned convolution (for long FIR filters)
    std::unique_ptr<MtxConvMaster> convolver_;
    AudioSampleBuffer convolverIn_, convolverOut_;
    bool useConvolver_ = false;
    int convolverLatency_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBand)
};

#endif // EQBAND_H_INCLUDED
