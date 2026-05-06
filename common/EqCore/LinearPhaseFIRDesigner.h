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

#ifndef LINEARPHASEFIRDESIGNER_H_INCLUDED
#define LINEARPHASEFIRDESIGNER_H_INCLUDED

#include <functional>
#include <vector>

namespace EqDesign
{

enum class FIRWindow
{
    Hann,
    Hamming,
    Blackman,
    BlackmanHarris,
    Kaiser,
    Rectangular
};

/** Design a symmetric (linear-phase) FIR filter from a target magnitude curve.

    Pipeline (frequency-sampling method):
      1. Sample |H_target(f)| on the linear FFT grid (firLength bins).
      2. Build a zero-phase complex spectrum (real-valued, mirror-symmetric).
      3. Inverse FFT → time-domain impulse response.
      4. Circular shift by firLength/2 to centre the impulse (causal Type-I/II FIR).
      5. Apply the chosen window.

    Parameters:
      magnitudeAt   target |H| as a function of physical frequency in Hz.
                    Should return a finite non-negative number; values < ~1e-6
                    are clamped to avoid log-domain blow-up in the IFFT.
      firLength     N (must be a power of two, ≥ 8).
      sampleRateHz  the sample rate at which the FIR will run.
      window        smoothing window applied to the truncated impulse response.
      kaiserBeta    only used when window == FIRWindow::Kaiser.

    Returns the N FIR coefficients (symmetric — h[i] == h[N-1-i] within
    floating-point round-off). The realised magnitude response approximates
    the target with smoothing introduced by the window. Group delay = (N−1)/2. */
std::vector<float>
designLinearPhaseFIR (std::function<double (double freqHz)> magnitudeAt,
                      int firLength,
                      double sampleRateHz,
                      FIRWindow window = FIRWindow::Kaiser,
                      double kaiserBeta = 8.0);

} // namespace EqDesign

#endif // LINEARPHASEFIRDESIGNER_H_INCLUDED
