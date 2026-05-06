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

#ifndef ANALOGPROTOTYPEDESIGNER_H_INCLUDED
#define ANALOGPROTOTYPEDESIGNER_H_INCLUDED

#include <array>
#include <complex>
#include <vector>

namespace EqDesign
{

using Cplx = std::complex<double>;

enum class Family
{
    Butterworth,
    Chebyshev1,
    Chebyshev2,
    Elliptic,
    Bessel
};

enum class Mode
{
    Lowpass,
    Highpass
};

/** Analog prototype filter, normalized so that the magnitude response
    is 1/sqrt(2) (≈ −3 dB) at ω = 1 rad/s.

    For all-pole families (Butterworth, Chebyshev I, Bessel) the zeros vector
    is empty (zeros at infinity). For Chebyshev II the zeros vector contains
    finite zeros on the imaginary axis. Poles always come in conjugate pairs
    plus a single real pole when the order is odd. */
struct Prototype
{
    std::vector<Cplx> poles;
    std::vector<Cplx> zeros;
    double gain = 1.0;
};

Prototype designButterworth (int order);
Prototype designChebyshev1  (int order, double passbandRippleDB);
Prototype designChebyshev2  (int order, double stopbandAttenDB);
Prototype designElliptic    (int order, double passbandRippleDB, double stopbandAttenDB);
Prototype designBessel      (int order);

/** Single biquad section in standard form:
    H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²) */
struct DigitalBiquad
{
    double b0, b1, b2;
    double a1, a2;
};

/** Design a digital biquad cascade for the given family/mode/order/cutoff.

    Pipeline:
      1. Build normalized analog prototype (cutoff at ω = 1 rad/s, see Prototype).
      2. Pre-warp cutoff to the analog domain to compensate the bilinear transform.
      3. LP→LP or LP→HP transformation in the analog domain.
      4. Bilinear transform of poles and zeros into the z-domain.
      5. Group conjugate pairs into second-order sections; pair a leftover real
         pole with a real zero (or with a zero at infinity when none remain).
      6. Normalize the overall gain so that LP has unity gain at DC and HP has
         unity gain at Nyquist.

    Throws nothing; returns an empty vector on invalid input (order < 1 or
    cutoff outside (0, fs/2)). */
std::vector<DigitalBiquad>
designDigitalCascade (Family family, Mode mode, int order,
                       double cutoffHz, double sampleRateHz,
                       double passbandRippleDB = 1.0,
                       double stopbandAttenDB  = 60.0);

} // namespace EqDesign

#endif // ANALOGPROTOTYPEDESIGNER_H_INCLUDED
