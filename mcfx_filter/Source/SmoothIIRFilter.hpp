/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#include "JuceHeader.h"

enum FilterType {
    LOW_PASS,
    HIGH_PASS,
    LOW_SHELF,
    HIGH_SHELF,
    PEAK
};

template <FilterType Type, int NumChannels, typename T = float>
class SmoothIIRFilter {

public:
    SmoothIIRFilter ()
    {
        setFilterParameters(T(48000), T(1000), T(0.707), T(1), false);

        reset();
    };
    ~SmoothIIRFilter () {};

    void setInterpolationTime (T sampleRate, T rampLengthInSeconds) {
        mSampleRate.setCurrentAndTargetValue(sampleRate);
        mFrequency.reset(sampleRate, rampLengthInSeconds);
        mQ.reset(sampleRate, rampLengthInSeconds);
        mGain.reset(sampleRate, rampLengthInSeconds);
    }

    void setFilterParameters(T sampleRate, T frequency, T Q, T gain, bool fade = true) noexcept {
        if (fade) {
            mSampleRate.setTargetValue(sampleRate);
            mFrequency.setTargetValue(frequency);
            mQ.setTargetValue(Q);
            mGain.setTargetValue(gain);
        } else {
            mSampleRate.setCurrentAndTargetValue(sampleRate);
            mFrequency.setCurrentAndTargetValue(frequency);
            mQ.setCurrentAndTargetValue(Q);
            mGain.setCurrentAndTargetValue(gain);

            calculateFilterCoefficients();
        }
    }

    void processBlock (AudioSampleBuffer& buffer) noexcept {

        auto const numSamples = buffer.getNumSamples();
        auto const numChannels = buffer.getNumChannels();

        // assert NumChannels < numChannels ?

        for (int i = 0; i < numSamples; i++) {
            // check whether we need to recalculate the coefficients
            if (mSampleRate.isSmoothing() ||
                mFrequency.isSmoothing() ||
                mQ.isSmoothing() ||
                mGain.isSmoothing()) {

                calculateFilterCoefficients();
            }
            for (int c = 0; c < numChannels; c++) {
                // apply the current filter to all channels // this can probably be SIMD accelerated!
                auto const x = buffer.getSample(c, i);
                auto const y = mB0 * x + mV1[c];
                buffer.setSample(c, i, y);

                // some intermediate values
                auto const t1 = mC1 * x + mV2[c];
                auto const t0 = -mA2 * mV1[c];

                // update states
                mV1[c] = -mA1 * mV1[c] + t1;
                mV2[c] = mC2 * x + t0;
            }
        }
    }

    void reset() noexcept {
        mV1.fill(T(0));
        mV2.fill(T(0));
    }

    T getB0() { return mCoeffs.coefficients[0]; };
    T getB1() { return mCoeffs.coefficients[1]; };
    T getB2() { return mCoeffs.coefficients[2]; };
    T getA1() { return mCoeffs.coefficients[3]; };
    T getA2() { return mCoeffs.coefficients[4]; };

private:

    // this is called in the hot loop!
    void calculateFilterCoefficients() noexcept {

        T const sampleRate = mSampleRate.getNextValue();
        T const frequency = mFrequency.getNextValue();
        T const Q = mQ.getNextValue();
        T const g = mGain.getNextValue();

        switch (Type) {
            case LOW_PASS:
                mCoeffs = mCoeffs.makeLowPass(sampleRate, frequency, Q);
                break;
            case HIGH_PASS:
                mCoeffs = mCoeffs.makeHighPass(sampleRate, frequency, Q);
                break;
            case LOW_SHELF:
                mCoeffs = mCoeffs.makeLowShelf(sampleRate, frequency, Q, g);
                break;
            case HIGH_SHELF:
                mCoeffs = mCoeffs.makeHighShelf(sampleRate, frequency, Q, g);
                break;
            case PEAK:
                mCoeffs = mCoeffs.makePeakFilter(sampleRate, frequency, Q, g);
                break;
            default:
                break;
        }
        auto const coeffs = mCoeffs.coefficients; // b0, b1, b2, a1, a2

        // do we need to care about a0? or is it anyway 1?

        // convert the filters into our modified TDF II !
        mB0 = coeffs[0];
        mA1 = coeffs[3];
        mA2 = coeffs[4];

        mC1 = (-mA1 * mB0) + coeffs[1]; // c1 = (-a1 * b0) + b1;
        mC2 = (-mA2 * mB0) + coeffs[2]; // c2 = -a2*b0 + b2;
    }

    // should we make them ValueSmoothingTypes::Multiplicative ? (f, Q, g) ?
    SmoothedValue<T, ValueSmoothingTypes::Linear> mSampleRate;
    SmoothedValue<T, ValueSmoothingTypes::Linear> mFrequency;
    SmoothedValue<T, ValueSmoothingTypes::Linear> mQ;
    SmoothedValue<T, ValueSmoothingTypes::Linear> mGain;

    // coefficients
    IIRCoefficients mCoeffs; // those are for the analyzer (outside world)
    T mB0, mA1, mA2, mC1, mC2;

    std::array<T, NumChannels> mV1, mV2;
};
