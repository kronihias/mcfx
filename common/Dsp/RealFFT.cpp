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

void RealFFT::transformRaw() noexcept
{
#if MCFX_REALFFT_VDSP
    DSPSplitComplex sc { re_.data(), im_.data() };

    // Pack the real frame as N/2 interleaved complex values, which is the
    // layout vDSP's real FFT consumes.
    vDSP_ctoz (reinterpret_cast<const DSPComplex*> (time_.data()), 2, &sc, 1,
               (vDSP_Length) (size_ / 2));
    vDSP_fft_zrip (static_cast<FFTSetup> (setup_), &sc, 1, (vDSP_Length) order_,
                   FFT_FORWARD);
#elif MCFX_REALFFT_FFTW
    fftwf_execute_dft_r2c (static_cast<fftwf_plan> (plan_), time_.data(),
                           static_cast<fftwf_complex*> (freq_));
#else
    juce::FloatVectorOperations::copy (scratch_.data(), time_.data(), size_);
    juce::FloatVectorOperations::clear (scratch_.data() + size_, size_);
    static_cast<juce::dsp::FFT*> (fft_)->performRealOnlyForwardTransform (scratch_.data(), true);
#endif
}

int RealFFT::binStride() const noexcept
{
#if MCFX_REALFFT_VDSP
    return 1;
#else
    return 2;
#endif
}

const float* RealFFT::realData() const noexcept
{
#if MCFX_REALFFT_VDSP
    return re_.data();
#elif MCFX_REALFFT_FFTW
    return reinterpret_cast<const float*> (freq_);
#else
    return scratch_.data();
#endif
}

const float* RealFFT::imagData() const noexcept
{
#if MCFX_REALFFT_VDSP
    return im_.data();
#elif MCFX_REALFFT_FFTW
    return reinterpret_cast<const float*> (freq_) + 1;
#else
    return scratch_.data() + 1;
#endif
}

void RealFFT::forward() noexcept
{
    if (! isReady())
        return;

    transformRaw();

    const int half = size_ / 2;

#if MCFX_REALFFT_VDSP
    // zrip returns twice the mathematical DFT; bring both parts back to it so
    // callers see the same numbers on every backend.
    const float halfScale = 0.5f;
    vDSP_vsmul (re_.data(), 1, &halfScale, re_.data(), 1, (vDSP_Length) half);
    vDSP_vsmul (im_.data(), 1, &halfScale, im_.data(), 1, (vDSP_Length) half);

    // Bin 0 carries DC in the real part and Nyquist in the imaginary part.
    // Lift Nyquist out and clear it, so bin 0 is the purely-real term it is.
    nyquist_ = im_[0];
    im_[0]   = 0.f;
#elif MCFX_REALFFT_FFTW
    const fftwf_complex* f = static_cast<const fftwf_complex*> (freq_);
    nyquist_ = f[half][0];
#else
    nyquist_ = scratch_[(size_t) (2 * half)];
#endif
}

void RealFFT::magnitudesSquared (float* magSqOut) noexcept
{
    if (! isReady() || magSqOut == nullptr)
        return;

    const int half = size_ / 2;

    transformRaw();

#if MCFX_REALFFT_VDSP
    DSPSplitComplex sc { re_.data(), im_.data() };

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
    // Already the mathematical DFT, and already only the non-negative half.
    const fftwf_complex* f = static_cast<const fftwf_complex*> (freq_);
    for (int k = 0; k <= half; ++k)
        magSqOut[k] = f[k][0] * f[k][0] + f[k][1] * f[k][1];

#else
    for (int k = 0; k <= half; ++k)
    {
        const float re = scratch_[(size_t) (2 * k)];
        const float im = scratch_[(size_t) (2 * k + 1)];
        magSqOut[k] = re * re + im * im;
    }
#endif
}

} // namespace mcfx
