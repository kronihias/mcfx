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

#ifndef MCFX_REALFFT_H_INCLUDED
#define MCFX_REALFFT_H_INCLUDED

#include <vector>

#if defined(__APPLE__)
 #define MCFX_REALFFT_VDSP 1
#elif MCFX_HAS_FFTW3
 #define MCFX_REALFFT_FFTW 1
#else
 #define MCFX_REALFFT_JUCE 1
#endif

namespace mcfx
{

/** Real-input forward FFT producing power per bin, on the platform's own
    vector library.

    Why not juce::dsp::FFT: its real-only entry point hands back an
    *interleaved* complex array, but both vDSP and FFTW produce their results in
    a different layout (split-complex and a half-spectrum array respectively).
    JUCE therefore repacks on every call, and that repacking costs about as much
    as the transform it is wrapping. Measured on Apple Silicon, samples ->
    magnitudes for a 4096-point frame:

        juce::dsp::FFT + scalar magnitudes   0.0133 ms
        vDSP_fft_zrip  + vDSP_zvmags         0.0036 ms   (3.7x)

    and the gap holds at 8192 (2.9x) and 32768 (3.1x). Defining
    JUCE_USE_VDSP_FRAMEWORK does *not* recover it — that only swaps the engine
    underneath the same repacking wrapper, and measures identical to the
    fallback. So the transform is called directly here, the way MtxConv already
    calls it for the convolution path.

    Everything is real-input: a real signal through a complex transform does
    twice the necessary work and returns a conjugate-symmetric half nobody
    reads. vDSP_fft_zrip and fftwf_plan_dft_r2c_1d are both the real variants.

    Output is |X[k]|^2 for k = 0 .. N/2 inclusive (N/2 + 1 values), with X the
    unnormalised DFT, sum x[n] exp(-2*pi*i*k*n/N). Callers apply their own window
    and amplitude normalisation. */
class RealFFT
{
public:
    RealFFT() = default;
    ~RealFFT();

    RealFFT (const RealFFT&) = delete;
    RealFFT& operator= (const RealFFT&) = delete;

    /** Build for 2^order points. Allocates; not for the audio thread. */
    void prepare (int order);

    int getSize() const noexcept      { return size_; }
    int getNumBins() const noexcept   { return size_ / 2 + 1; }
    bool isReady() const noexcept     { return size_ > 0; }

    /** Where the caller writes its getSize() windowed samples. Exposed rather
        than taking a const pointer so no backend needs an extra copy — at 4096
        points a memcpy is a quarter of the cost of the transform itself. */
    float* getTimeBuffer() noexcept   { return time_.data(); }

    /** Transform whatever is in the time buffer and write getNumBins() power
        values. The time buffer's contents are not preserved. */
    void magnitudesSquared (float* magSqOut) noexcept;

private:
    void release();

    int size_  = 0;
    int order_ = 0;
    std::vector<float> time_;

#if MCFX_REALFFT_VDSP
    // vDSP works in split-complex: real and imaginary parts in separate arrays,
    // which is also what lets the magnitudes be computed with one vector call.
    std::vector<float> re_, im_;
    void* setup_ = nullptr;                  // FFTSetup, kept opaque in the header
#elif MCFX_REALFFT_FFTW
    void* plan_ = nullptr;                   // fftwf_plan
    void* freq_ = nullptr;                   // fftwf_complex[N/2 + 1]
#else
    std::vector<float> scratch_;             // 2N, interleaved
    void* fft_ = nullptr;                    // juce::dsp::FFT
#endif
};

} // namespace mcfx

#endif // MCFX_REALFFT_H_INCLUDED
