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

#ifndef MCFX_CONSTANTQ_H_INCLUDED
#define MCFX_CONSTANTQ_H_INCLUDED

#include "RealFFT.h"
#include <complex>
#include <vector>

namespace mcfx
{

/** Constant-Q transform of one frame, by the Brown-Puckette spectral-kernel
    method: each bin's windowed complex exponential is transformed once at
    prepare() time and kept as a sparse spectrum, so a frame costs one real FFT
    plus a short dot product per bin instead of a transform per bin.

    Fixed bins per octave, so the window lengthens towards the bass (fine
    pitch resolution) and shortens towards the treble (fast transients). Below
    getConstantQFloorHz() the ideal window would outrun the frame; it is capped
    there and Q falls off gradually. A capped kernel keeps its true centre
    frequency — deriving it from the window length instead would collapse every
    capped bin onto one frequency.

    Where the windows sit in the frame decides what an impulse looks like:

    - Alignment::End — every window ends at the newest sample. Lowest latency
      for the treble, which is what a spectrum line wants. But a window's
      response to an impulse peaks at its centre, half its length back, so in a
      spectrogram the long bass windows report a click up to half the frame
      later than the treble does: the click bends.

    - Alignment::Centred — every window is centred on the same instant, the
      middle of the frame. A click then peaks in every bin at once and is drawn
      as a vertical line, with the bass smear symmetric about it instead of
      trailing it. The price is latency: the frame describes the instant
      getCentreFromEnd() samples before its newest sample. This is the fix
      worked out in SpaceLevelMeter, done here inside a single frame (every
      kernel shares the frame's centre) rather than by delaying columns, so
      it is exact to the sample and needs no history.

    Magnitudes are calibrated so a full-scale sine reads 1.0 in its bin. */
class ConstantQ
{
public:
    enum class Alignment { End, Centred };

    struct Config
    {
        double    sampleRate    = 48000.0;
        int       fftOrder      = 15;          // frame length 2^order
        double    fMin          = 20.0;
        double    fMax          = 20000.0;     // clamped to 0.45 * sampleRate
        int       binsPerOctave = 24;          // how densely bins are placed
        /** How sharp each bin is, as the bins per octave its Q would tile
            exactly; 0 = binsPerOctave. Placing bins more densely than this
            (e.g. 48 placed, 24 resolution) costs little and keeps a tone
            between bins from reading up to ~1.8 dB low, which is what a
            Hamming window loses half a bin off centre. */
        int       resolutionBinsPerOctave = 0;
        Alignment alignment     = Alignment::End;
        /** Kernel spectrum entries below this fraction of the peak are
            dropped. 2e-2 keeps ~215 of 32768 bins per kernel for ~0.02 dB. */
        float     sparsity      = 2.0e-2f;

        bool operator== (const Config& o) const
        {
            return sampleRate == o.sampleRate && fftOrder == o.fftOrder && fMin == o.fMin
                && fMax == o.fMax && binsPerOctave == o.binsPerOctave
                && resolutionBinsPerOctave == o.resolutionBinsPerOctave
                && alignment == o.alignment && sparsity == o.sparsity;
        }
        bool operator!= (const Config& o) const { return ! (*this == o); }
    };

    ConstantQ() = default;
    ConstantQ (const ConstantQ&) = delete;
    ConstantQ& operator= (const ConstantQ&) = delete;

    /** Build the kernels. Expensive (one FFT per bin) — not for the audio
        thread. Rebuilds only when the config changed. */
    void prepare (const Config& config);

    bool   isReady() const          { return numBins_ > 0 && fft_.isReady(); }
    const Config& getConfig() const { return config_; }
    int    getFrameSize() const     { return fft_.getSize(); }
    int    getNumBins() const       { return numBins_; }
    double getBinFrequency (int k) const;
    double getConstantQFloorHz() const;

    /** Samples between the instant a frame describes and its newest sample:
        0 for Alignment::End, half the frame for Alignment::Centred. */
    int getCentreFromEnd() const;

    /** Write getFrameSize() samples here, oldest first, then call transform(). */
    float* getTimeBuffer() { return fft_.getTimeBuffer(); }

    /** Transform the time buffer into getNumBins() linear magnitudes. The time
        buffer's contents are not preserved. */
    void transform (float* magnitudesOut);

    /** Linear magnitude at a frequency from a transform() result, interpolated
        between bins in log frequency and held at the ends. */
    float magnitudeAt (const float* magnitudes, double freqHz) const;

private:
    void buildKernels();
    double q() const;

    Config config_;
    bool   prepared_ = false;
    int    numBins_  = 0;
    double fMax_     = 0.0;

    RealFFT fft_;

    // Sparse kernels, flattened: bin k spans [offset_[k], offset_[k+1]).
    std::vector<int>                  kernelIndex_;
    std::vector<std::complex<float>>  kernelValue_;
    std::vector<int>                  kernelOffset_;
};

} // namespace mcfx

#endif // MCFX_CONSTANTQ_H_INCLUDED
