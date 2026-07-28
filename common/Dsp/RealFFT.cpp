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

// Accelerate must come before JuceHeader: MacTypes.h declares a struct Point
// that collides with juce::Point if JUCE is pulled in first.
#include "RealFFT.h"

#if MCFX_REALFFT_VDSP
 #include <Accelerate/Accelerate.h>
#elif MCFX_REALFFT_FFTW
 #include <fftw3.h>
 #include "Fftw/FftwPlanner.h"
#else
 #include "JuceHeader.h"
#endif

#include <cmath>

namespace mcfx
{

RealFFT::~RealFFT()
{
    release();
}

void RealFFT::release()
{
#if MCFX_REALFFT_VDSP
    if (setup_ != nullptr)
    {
        vDSP_destroy_fftsetup (static_cast<FFTSetup> (setup_));
        setup_ = nullptr;
    }
#elif MCFX_REALFFT_FFTW
    if (plan_ != nullptr)
    {
        fftwf_destroy_plan (static_cast<fftwf_plan> (plan_));
        plan_ = nullptr;
    }
    if (freq_ != nullptr)
    {
        fftwf_free (freq_);
        freq_ = nullptr;
    }
#else
    delete static_cast<juce::dsp::FFT*> (fft_);
    fft_ = nullptr;
#endif
    size_ = 0;
}

void RealFFT::prepare (int order)
{
    if (order == order_ && isReady())
        return;

    release();

    order_ = order;
    size_  = 1 << order;
    const int half = size_ / 2;

    time_.assign ((size_t) size_, 0.f);

#if MCFX_REALFFT_VDSP
    re_.assign ((size_t) half, 0.f);
    im_.assign ((size_t) half, 0.f);
    setup_ = vDSP_create_fftsetup ((vDSP_Length) order_, FFT_RADIX2);
#elif MCFX_REALFFT_FFTW
    // FFTW's planner is process-global and not thread-safe.
    ensureFftwPlannerThreadSafe();
    freq_ = fftwf_malloc (sizeof (fftwf_complex) * (size_t) (half + 1));
    plan_ = fftwf_plan_dft_r2c_1d (size_, time_.data(),
                                   static_cast<fftwf_complex*> (freq_),
                                   FFTW_MEASURE | FFTW_PRESERVE_INPUT);
#else
    scratch_.assign ((size_t) (2 * size_), 0.f);
    fft_ = new juce::dsp::FFT (order_);
#endif
}

void RealFFT::magnitudesSquared (float* magSqOut) noexcept
{
    if (! isReady() || magSqOut == nullptr)
        return;

    const int half = size_ / 2;

#if MCFX_REALFFT_VDSP
    DSPSplitComplex sc { re_.data(), im_.data() };

    // Pack the real frame as N/2 interleaved complex values, which is the
    // layout vDSP's real FFT consumes.
    vDSP_ctoz (reinterpret_cast<const DSPComplex*> (time_.data()), 2, &sc, 1,
               (vDSP_Length) half);
    vDSP_fft_zrip (static_cast<FFTSetup> (setup_), &sc, 1, (vDSP_Length) order_,
                   FFT_FORWARD);

    // One vector call for every bin's power.
    vDSP_zvmags (&sc, 1, magSqOut, 1, (vDSP_Length) half);

    // vDSP_fft_zrip returns twice the mathematical DFT, so power is 4x.
    const float quarter = 0.25f;
    vDSP_vsmul (magSqOut, 1, &quarter, magSqOut, 1, (vDSP_Length) half);

    // Bin 0 is a special case in this packing: zrip stores the two purely-real
    // terms together, DC in realp[0] and Nyquist in imagp[0]. vDSP_zvmags just
    // summed them into one bogus value, so both are rebuilt here.
    const float dc  = 0.5f * re_[0];
    const float nyq = 0.5f * im_[0];
    magSqOut[0]    = dc  * dc;
    magSqOut[half] = nyq * nyq;

#elif MCFX_REALFFT_FFTW
    fftwf_execute_dft_r2c (static_cast<fftwf_plan> (plan_), time_.data(),
                           static_cast<fftwf_complex*> (freq_));

    // Already the mathematical DFT, and already only the non-negative half.
    const fftwf_complex* f = static_cast<const fftwf_complex*> (freq_);
    for (int k = 0; k <= half; ++k)
        magSqOut[k] = f[k][0] * f[k][0] + f[k][1] * f[k][1];

#else
    juce::FloatVectorOperations::copy (scratch_.data(), time_.data(), size_);
    juce::FloatVectorOperations::clear (scratch_.data() + size_, size_);
    static_cast<juce::dsp::FFT*> (fft_)->performRealOnlyForwardTransform (scratch_.data(), true);

    for (int k = 0; k <= half; ++k)
    {
        const float re = scratch_[(size_t) (2 * k)];
        const float im = scratch_[(size_t) (2 * k + 1)];
        magSqOut[k] = re * re + im * im;
    }
#endif
}

} // namespace mcfx
