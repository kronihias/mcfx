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
    RawBiquad
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
    void reset();

    /** Copy parameters from source without resetting filter state.
        Triggers smooth coefficient transition for click-free updates. */
    void syncParametersFrom(const EqBand& source);

    // --- Frequency response ---
    // Returns complex response at frequency f (Hz) for magnitude+phase
    // If alwaysCompute is true, returns the response even when disabled
    std::complex<float> getFrequencyResponse(double freqHz, bool alwaysCompute = false) const;

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
