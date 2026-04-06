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

#include "EqChain.h"

EqChain::EqChain()
{
}

EqChain::~EqChain()
{
}

EqBand* EqChain::addBand()
{
    auto* band = new EqBand();
    band->prepare(sampleRate_, maxBlockSize_);
    bands_.add(band);
    return band;
}

void EqChain::removeBand(int index)
{
    if (index >= 0 && index < bands_.size())
        bands_.remove(index);
}

void EqChain::moveBand(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < bands_.size() &&
        toIndex >= 0 && toIndex < bands_.size() &&
        fromIndex != toIndex)
    {
        bands_.move(fromIndex, toIndex);
    }
}

EqBand* EqChain::getBand(int index) const
{
    if (index >= 0 && index < bands_.size())
        return bands_[index];
    return nullptr;
}

void EqChain::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;
    for (auto* band : bands_)
        band->prepare(sampleRate, maxBlockSize);
}

void EqChain::processBlock(float* data, int numSamples)
{
    for (auto* band : bands_)
        band->processBlock(data, numSamples);
}

void EqChain::reset()
{
    for (auto* band : bands_)
        band->reset();
}

void EqChain::syncParametersFrom(const EqChain& source)
{
    // Only sync if band count matches — otherwise a full rebuild is needed
    if (bands_.size() != source.bands_.size())
        return;

    for (int i = 0; i < bands_.size(); ++i)
        bands_[i]->syncParametersFrom(*source.bands_[i]);
}

int EqChain::getConvolverLatency() const
{
    int total = 0;
    for (auto* band : bands_)
        total += band->getConvolverLatency();
    return total;
}

std::complex<float> EqChain::getFrequencyResponse(double freqHz) const
{
    std::complex<float> combined(1.f, 0.f);
    for (auto* band : bands_)
        combined *= band->getFrequencyResponse(freqHz);
    return combined;
}

var EqChain::toJson() const
{
    Array<var> arr;
    for (auto* band : bands_)
        arr.add(band->toJson());
    return arr;
}

void EqChain::fromJson(const var& json, double sampleRate)
{
    bands_.clear();
    sampleRate_ = sampleRate;

    if (json.isArray())
    {
        for (int i = 0; i < json.size(); ++i)
        {
            auto* band = EqBand::fromJson(json[i]);
            band->prepare(sampleRate_, maxBlockSize_);
            bands_.add(band);
        }
    }
}
