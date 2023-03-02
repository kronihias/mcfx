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

#ifndef __mcfx_filter__SmoothIIRFilter__
#define __mcfx_filter__SmoothIIRFilter__

#include "JuceHeader.h"

class SmoothIIRFilter : public IIRFilter {


public:
    SmoothIIRFilter();
    SmoothIIRFilter (const SmoothIIRFilter& other) noexcept;

    ~SmoothIIRFilter();

    void setInterpolationSamples(int num); // set the number of samples for interpolation

    void setCoefficients (const IIRCoefficients& newCoefficients) noexcept;


    void processSamples (float* const samples, const int numSamples) noexcept;

private:

    int numInterpolationSamples_;
    int currentInterpolationSample_;

    float dCoef_[5]; // interpolation step size

    IIRCoefficients coefficients_; // the old coefficients

    bool needInterpolation_;

};

#endif /* defined(__mcfx_filter__SmoothIIRFilter__) */
