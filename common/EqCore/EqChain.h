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

#ifndef EQCHAIN_H_INCLUDED
#define EQCHAIN_H_INCLUDED

#include "EqBand.h"

class EqChain
{
public:
    EqChain();
    ~EqChain();

    // Band management
    EqBand* addBand();
    void removeBand(int index);
    void moveBand(int fromIndex, int toIndex);
    EqBand* getBand(int index) const;
    int getNumBands() const { return bands_.size(); }

    // Processing
    void prepare(double sampleRate, int maxBlockSize);
    void processBlock(float* data, int numSamples);
    void reset();

    /** Sync parameters from source chain (same number of bands required).
        Preserves filter state for click-free parameter updates. */
    void syncParametersFrom(const EqChain& source);

    /** Total convolver latency across all bands (serial processing = latencies add). */
    int getConvolverLatency() const;

    // Frequency response — combined response of all enabled bands
    std::complex<float> getFrequencyResponse(double freqHz) const;

    // JSON
    var toJson() const;
    void fromJson(const var& json, double sampleRate);

private:
    OwnedArray<EqBand> bands_;
    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqChain)
};

#endif // EQCHAIN_H_INCLUDED
