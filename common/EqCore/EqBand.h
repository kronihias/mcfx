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
#include <array>
#include <complex>
#include <vector>

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
    CrossoverAP
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

    // --- Raw IIR coefficients (direct biquad) ---
    struct BiquadCoeffs { float b0, b1, b2, a0, a1, a2; };
    bool hasRawCoefficients() const { return hasRawCoeffs_; }
    BiquadCoeffs getRawCoefficients() const { return rawCoeffs_; }
    void setRawCoefficients(float b0, float b1, float b2, float a0, float a1, float a2);

    // --- Delay ---
    int getDelaySamples() const { return delaySamples_; }
    void setDelaySamples(int samples);

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

    // --- Frequency response ---
    // Returns complex response at frequency f (Hz) for magnitude+phase
    // If alwaysCompute is true, returns the response even when disabled
    std::complex<float> getFrequencyResponse(double freqHz, bool alwaysCompute = false) const;

    // --- JSON ---
    var toJson() const;
    static EqBand* fromJson(const var& json);

    bool usesCascade() const;

private:
    void updateIIRCoefficients();
    void applyIIR(float* data, int numSamples);
    void applyCascadeIIR(float* data, int numSamples);
    void applyFIR(float* data, int numSamples);
    void applyGain(float* data, int numSamples);
    void applyDelay(float* data, int numSamples);

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
    std::vector<std::array<float, 5>> cascadeCoeffs_;  // [b0,b1,b2,a1,a2] per section
    std::vector<std::array<float, 2>> cascadeState_;    // [z1,z2] per section

    // Biquad state (transposed direct form II)
    IIRCoefficients iirCoeffs_;
    float iirState_[2] = { 0.f, 0.f }; // z^-1 states
    double sampleRate_ = 48000.0;

    // FIR
    std::vector<float> firCoeffs_;
    std::vector<float> firState_; // circular buffer for FIR convolution

    // Gain
    float linearGain_ = 1.f;

    // Delay
    int delaySamples_ = 0;
    std::vector<float> delayBuffer_;
    int delayWritePos_ = 0;

    // Channel routing
    int inputChannel_ = -1;  // -1 = not set (diagonal)
    int outputChannel_ = -1;
    bool diagonal_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqBand)
};

#endif // EQBAND_H_INCLUDED
