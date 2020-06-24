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

#include "SmoothIIRFilter.h"

#if JUCE_INTEL
#define JUCE_SNAP_TO_ZERO(n)    if (! (n < -1.0e-8f || n > 1.0e-8f)) n = 0;
#else
#define JUCE_SNAP_TO_ZERO(n)
#endif

SmoothIIRFilter::SmoothIIRFilter() :    numInterpolationSamples_(1024),
                                        currentInterpolationSample_(0),
                                        needInterpolation_(false)
{
    zeromem (dCoef_, sizeof (dCoef_));
    v1 = 0.f;
    v2 = 0.f;
    active = false;
}

SmoothIIRFilter::SmoothIIRFilter (const SmoothIIRFilter& other) noexcept
{
    zeromem (dCoef_, sizeof (dCoef_));
    
    v1 = 0.f;
    v2 = 0.f;
    active = other.active;
    
    const SpinLock::ScopedLockType sl (other.processLock);
    coefficients = other.coefficients;
}

SmoothIIRFilter::~SmoothIIRFilter()
{
    
}

void SmoothIIRFilter::setInterpolationSamples(int num)
{
    numInterpolationSamples_ = num;
}

void SmoothIIRFilter::setCoefficients (const IIRCoefficients& newCoefficients) noexcept
{
    const SpinLock::ScopedLockType sl (processLock);
    
    currentInterpolationSample_ = 0;
    
    coefficients = newCoefficients;
    
    // calculate the differential for interpolation
    dCoef_[0] = (coefficients.coefficients[0] - coefficients_.coefficients[0]) / numInterpolationSamples_;
    dCoef_[1] = (coefficients.coefficients[1] - coefficients_.coefficients[1]) / numInterpolationSamples_;
    dCoef_[2] = (coefficients.coefficients[2] - coefficients_.coefficients[2]) / numInterpolationSamples_;
    dCoef_[3] = (coefficients.coefficients[3] - coefficients_.coefficients[3]) / numInterpolationSamples_;
    dCoef_[4] = (coefficients.coefficients[4] - coefficients_.coefficients[4]) / numInterpolationSamples_;
    
    needInterpolation_ = true;
    active = true;
}

void SmoothIIRFilter::processSamples (float* const samples, const int numSamples) noexcept
{
    const SpinLock::ScopedLockType sl (processLock);
    
    if (active)
    {
        if (!needInterpolation_)
        {
            const float c0 = coefficients.coefficients[0];
            const float c1 = coefficients.coefficients[1];
            const float c2 = coefficients.coefficients[2];
            const float c3 = coefficients.coefficients[3];
            const float c4 = coefficients.coefficients[4];
            float lv1 = v1, lv2 = v2;
            
            for (int i = 0; i < numSamples; ++i)
            {
                const float in = samples[i];
                const float out = c0 * in + lv1;
                samples[i] = out;
                
                lv1 = c1 * in - c3 * out + lv2;
                lv2 = c2 * in - c4 * out;
            }
            
            JUCE_SNAP_TO_ZERO (lv1);  v1 = lv1;
            JUCE_SNAP_TO_ZERO (lv2);  v2 = lv2;
            
        }
        else // do interpolation
        {
            const float d_c0 = dCoef_[0];
            const float d_c1 = dCoef_[1];
            const float d_c2 = dCoef_[2];
            const float d_c3 = dCoef_[3];
            const float d_c4 = dCoef_[4];
            
            float *c0 = &coefficients_.coefficients[0];
            float *c1 = &coefficients_.coefficients[1];
            float *c2 = &coefficients_.coefficients[2];
            float *c3 = &coefficients_.coefficients[3];
            float *c4 = &coefficients_.coefficients[4];
            
            float lv1 = v1, lv2 = v2;
            
            for (int i = 0; i < numSamples; ++i)
            {
                if (needInterpolation_)
                {
                    (*c0) += d_c0;
                    (*c1) += d_c1;
                    (*c2) += d_c2;
                    (*c3) += d_c3;
                    (*c4) += d_c4;
                }
                
                const float in = samples[i];
                const float out = (*c0) * in + lv1;
                samples[i] = out;
                
                lv1 = (*c1) * in - (*c3) * out + lv2;
                lv2 = (*c2) * in - (*c4) * out;
                
                if (++currentInterpolationSample_ >= numInterpolationSamples_)
                {
                    
                    coefficients_ = coefficients;
                    needInterpolation_ = false;
                }
                
            }
            
            JUCE_SNAP_TO_ZERO (lv1);  v1 = lv1;
            JUCE_SNAP_TO_ZERO (lv2);  v2 = lv2;
        }
        
    }
}


#undef JUCE_SNAP_TO_ZERO
