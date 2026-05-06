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

#include "LinearPhaseFIRDesigner.h"

#include "JuceHeader.h"

#include <algorithm>
#include <cmath>

namespace EqDesign
{

namespace
{
// N must be a power of two; round up if not.
int nextPow2 (int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

int log2Int (int n)
{
    int p = 0;
    while ((1 << p) < n) ++p;
    return p;
}

juce::dsp::WindowingFunction<float>::WindowingMethod toJuceWindow (FIRWindow w)
{
    using WF = juce::dsp::WindowingFunction<float>;
    switch (w)
    {
        case FIRWindow::Hann:           return WF::hann;
        case FIRWindow::Hamming:        return WF::hamming;
        case FIRWindow::Blackman:       return WF::blackman;
        case FIRWindow::BlackmanHarris: return WF::blackmanHarris;
        case FIRWindow::Kaiser:         return WF::kaiser;
        case FIRWindow::Rectangular:    return WF::rectangular;
    }
    return WF::hann;
}
} // anonymous namespace

std::vector<float>
designLinearPhaseFIR (std::function<double (double freqHz)> magnitudeAt,
                      int firLength,
                      double sampleRateHz,
                      FIRWindow window,
                      double kaiserBeta)
{
    if (firLength < 8 || sampleRateHz <= 0.0 || ! magnitudeAt)
        return {};

    // Force odd length so the result is a Type-I FIR with h[i] == h[N-1-i] exactly
    // (group delay = (N-1)/2, an integer). This works for every filter shape — Type-II
    // (even N) would force the response to 0 at Nyquist, which is fine for LP but
    // breaks HP / HighShelf.
    int N = firLength;
    if ((N & 1) == 0) --N;

    // FFT size: smallest power of two ≥ N. For N = 2^k − 1 (the lengths the dialog
    // exposes) this is exactly 2^k → no efficiency penalty.
    const int M = nextPow2 (N);
    juce::dsp::FFT fft (log2Int (M));

    // Sample target magnitude on the FFT grid. performRealOnlyInverseTransform
    // *reads* only M/2+1 complex bins from the first M+2 floats, but the docs
    // require the buffer to be 2*M floats long because some FFT backends use
    // the extra space as scratch — passing a smaller buffer corrupts the heap.
    std::vector<float> buf ((size_t) (2 * M), 0.0f);
    for (int k = 0; k <= M / 2; ++k)
    {
        double f = (double) k * sampleRateHz / (double) M;
        double mag = magnitudeAt (f);
        if (! std::isfinite (mag) || mag < 0.0) mag = 0.0;
        buf[(size_t) (2 * k)]     = (float) mag;   // real
        buf[(size_t) (2 * k + 1)] = 0.0f;           // imag = 0  →  zero phase
    }

    fft.performRealOnlyInverseTransform (buf.data());

    // The IFFT output is symmetric around index 0 (with mod-M wraparound). Pull
    // the central N samples — indices [-half .. +half] from index 0 — into the
    // output, producing a causal Type-I FIR (peak at index half = (N-1)/2).
    const int half = (N - 1) / 2;
    std::vector<float> coeffs ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        int src = ((i - half) % M + M) % M;
        coeffs[(size_t) i] = buf[(size_t) src];
    }

    // Window
    juce::dsp::WindowingFunction<float> win ((size_t) N,
                                             toJuceWindow (window),
                                             /*normalise*/ false,
                                             (float) kaiserBeta);
    win.multiplyWithWindowingTable (coeffs.data(), (size_t) N);

    return coeffs;
}

} // namespace EqDesign
