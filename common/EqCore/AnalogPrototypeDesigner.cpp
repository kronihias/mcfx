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

#include "AnalogPrototypeDesigner.h"

#include "JuceHeader.h"           // for juce::dsp::SpecialFunctions (elliptic)

#include <algorithm>
#include <cmath>

namespace EqDesign
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

// Bessel filter analog poles, normalized so |H(jω=1)| = 1/√2 (−3 dB at ω = 1 rad/s).
// Each entry is one pole; conjugate pairs are listed as the upper-half-plane pole only,
// so the designer mirrors them. Orders 1–10. Source: standard reference tables
// (Williams "Electronic Filter Design Handbook"; scipy.signal.bessel norm='mag').
struct BesselTableEntry { int order; std::vector<Cplx> upperPoles; bool hasRealPole; double realPole; };

const BesselTableEntry kBesselTable[] = {
    { 1, {}, true,  -1.0 },
    { 2, { Cplx(-1.10160133059,  0.63600982475) }, false, 0.0 },
    { 3, { Cplx(-1.04739717666,  0.99957396293) }, true,  -1.32267579991 },
    { 4, { Cplx(-1.37006783055,  0.41059703631),
           Cplx(-0.99520876435,  1.25710573454) }, false, 0.0 },
    { 5, { Cplx(-1.38087732586,  0.71790958762),
           Cplx(-0.95808793519,  1.47097004940) }, true,  -1.50231627145 },
    { 6, { Cplx(-1.57149040362,  0.32089637610),
           Cplx(-1.38185809760,  0.97179121848),
           Cplx(-0.93098497159,  1.66186326778) }, false, 0.0 },
    { 7, { Cplx(-1.61203876622,  0.58901214901),
           Cplx(-1.37890321680,  1.19156677780),
           Cplx(-0.90986778091,  1.83645135304) }, true,  -1.68436817428 },
    { 8, { Cplx(-1.75740840040,  0.27286140371),
           Cplx(-1.63693941813,  0.82272674517),
           Cplx(-1.37384121764,  1.38835657588),
           Cplx(-0.89286971500,  1.99832361470) }, false, 0.0 },
    { 9, { Cplx(-1.78937639789,  0.51261843134),
           Cplx(-1.65239648458,  1.03138956030),
           Cplx(-1.36490198385,  1.56839491022),
           Cplx(-0.87837526100,  2.14946812823) }, true,  -1.85660380282 },
    {10, { Cplx(-1.92761969400,  0.24162347380),
           Cplx(-1.81256952760,  0.72701044867),
           Cplx(-1.66761870775,  1.22110021825),
           Cplx(-1.36069227838,  1.74362111957),
           Cplx(-0.86575690307,  2.29260483098) }, false, 0.0 }
};

// Build a full pole list (with conjugates) from the upper-half table entry.
std::vector<Cplx> expandFromTable (const BesselTableEntry& e)
{
    std::vector<Cplx> poles;
    poles.reserve (e.upperPoles.size() * 2 + (e.hasRealPole ? 1 : 0));
    if (e.hasRealPole)
        poles.push_back (Cplx (e.realPole, 0.0));
    for (auto& p : e.upperPoles)
    {
        poles.push_back (p);
        poles.push_back (std::conj (p));
    }
    return poles;
}
} // anonymous namespace

//==============================================================================
// Analog prototype construction
//==============================================================================

Prototype designButterworth (int order)
{
    Prototype p;
    if (order < 1) return p;

    // Poles equally spaced on the left half of the unit circle.
    // For k = 1..N: angle = π/2 + (2k-1)π/(2N).
    for (int k = 1; k <= order; ++k)
    {
        double theta = kPi * 0.5 + (2.0 * k - 1.0) * kPi / (2.0 * order);
        p.poles.emplace_back (std::cos (theta), std::sin (theta));
    }
    p.gain = 1.0;
    return p;
}

Prototype designChebyshev1 (int order, double passbandRippleDB)
{
    Prototype p;
    if (order < 1) return p;

    double rippleDB = std::max (0.001, passbandRippleDB);
    double eps    = std::sqrt (std::pow (10.0, rippleDB / 10.0) - 1.0);
    double mu     = std::asinh (1.0 / eps) / (double) order;
    double sinhMu = std::sinh (mu);
    double coshMu = std::cosh (mu);

    // Passband-edge-normalized poles.
    for (int k = 1; k <= order; ++k)
    {
        double phi = (2.0 * k - 1.0) * kPi / (2.0 * order);
        p.poles.emplace_back (-sinhMu * std::sin (phi), coshMu * std::cos (phi));
    }

    // Rescale so the −3 dB frequency lands at ω = 1 (consistent with all other families).
    // Solve T_N(ω_3dB) = 1/ε for ω_3dB:
    //   ε < 1 (ripple < 3.0103 dB): the −3 dB point lies *outside* the ripple band, so
    //     T_N is in its hyperbolic regime → ω_3dB = cosh(acosh(1/ε)/N).
    //   ε > 1 (ripple > 3.0103 dB): the −3 dB point falls *inside* the ripple band, so
    //     T_N is in its trig regime → ω_3dB = cos(acos(1/ε)/N) (with 1/ε ≤ 1, valid).
    //   ε = 1 exactly: −3 dB sits at the passband edge ω = 1.
    double w3dB;
    if (eps < 1.0)
        w3dB = std::cosh (std::acosh (1.0 / eps) / (double) order);
    else if (eps > 1.0)
        w3dB = std::cos  (std::acos  (1.0 / eps) / (double) order);
    else
        w3dB = 1.0;
    for (auto& pole : p.poles) pole /= w3dB;

    // Gain: peak passband magnitude is 1 (Chebyshev's natural max). Compute the
    // analog DC gain from the product of pole magnitudes — needed to renormalize
    // the digital cascade later.
    Cplx num (1.0, 0.0);
    for (auto& pole : p.poles) num *= -pole;     // ∏(-pᵢ) = H(0)·∏(-pᵢ)/∏(-pᵢ) = H(0) when no zeros
    double dcAbs = std::abs (num);
    // For even N the natural Cheby1 DC gain is 1/√(1+ε²) of the peak; we keep peak = 1.
    if ((order & 1) == 0)
        dcAbs /= std::sqrt (1.0 + eps * eps);
    p.gain = dcAbs;
    return p;
}

Prototype designChebyshev2 (int order, double stopbandAttenDB)
{
    Prototype p;
    if (order < 1) return p;

    double Adb = std::max (0.1, stopbandAttenDB);
    double eps = 1.0 / std::sqrt (std::pow (10.0, Adb / 10.0) - 1.0);
    double mu     = std::asinh (1.0 / eps) / (double) order;
    double sinhMu = std::sinh (mu);
    double coshMu = std::cosh (mu);

    // Cheby1-style pole locations, then take their reciprocal to get Cheby2 poles
    // (stopband-edge normalization, ωs = 1).
    for (int k = 1; k <= order; ++k)
    {
        double phi = (2.0 * k - 1.0) * kPi / (2.0 * order);
        Cplx q (-sinhMu * std::sin (phi), coshMu * std::cos (phi));
        p.poles.push_back (1.0 / q);
    }

    // Zeros on the imaginary axis at j / cos(φ_k), skip the middle one when order is odd.
    for (int k = 1; k <= order; ++k)
    {
        double phi = (2.0 * k - 1.0) * kPi / (2.0 * order);
        double c = std::cos (phi);
        if (std::abs (c) < 1e-14) continue;       // odd-order central zero is at infinity
        p.zeros.emplace_back (0.0, 1.0 / c);
    }

    // Push the −3 dB point from cosh(...)⁻¹ < 1 up to ω = 1.
    double w3dB = 1.0 / std::cosh (std::acosh (1.0 / eps) / (double) order);
    for (auto& pole : p.poles) pole /= w3dB;
    for (auto& zero : p.zeros) zero /= w3dB;

    // DC gain of a Cheby2 LP is 1 by construction, so we want the analog
    // gain factor to make H(0) = 1: g = ∏(-pᵢ) / ∏(-zᵢ).
    Cplx denom (1.0, 0.0);
    for (auto& pole : p.poles) denom *= -pole;
    Cplx numer (1.0, 0.0);
    for (auto& zero : p.zeros) numer *= -zero;
    p.gain = std::abs (denom / numer);
    return p;
}

Prototype designBessel (int order)
{
    Prototype p;
    if (order < 1) return p;
    if (order > 10) order = 10;

    for (auto& e : kBesselTable)
    {
        if (e.order == order)
        {
            p.poles = expandFromTable (e);
            // All-pole filter; gain = 1 by construction (the table is normalized
            // to give |H(0)| = 1).
            p.gain = 1.0;
            return p;
        }
    }
    return p;
}

namespace
{
// Evaluate |H(jω)| for an analog prototype (poles, zeros, gain).
double analogMagnitude (const Prototype& proto, double w)
{
    Cplx s (0.0, w);
    Cplx num (proto.gain, 0.0);
    for (auto& z : proto.zeros) num *= (s - z);
    Cplx den (1.0, 0.0);
    for (auto& p : proto.poles) den *= (s - p);
    return std::abs (num / den);
}
} // anonymous namespace

Prototype designElliptic (int order, double passbandRippleDB, double stopbandAttenDB)
{
    Prototype proto;
    if (order < 1) return proto;

    // N = 1 degenerates to a single-pole all-pole filter (no transition zero possible).
    if (order == 1)
    {
        proto.poles.emplace_back (-1.0, 0.0);
        proto.gain = 1.0;
        return proto;
    }

    using juce::dsp::SpecialFunctions;
    using juce::dsp::Complex;

    double Rp = std::max (0.001, passbandRippleDB);
    double Rs = std::max (Rp + 1.0, stopbandAttenDB);   // ensure Rs > Rp so k1 < 1

    double Gp = std::pow (10.0,  -Rp / 20.0);
    double Gs = std::pow (10.0,  -Rs / 20.0);
    double eps_p = std::sqrt (1.0 / (Gp * Gp) - 1.0);
    double eps_s = std::sqrt (1.0 / (Gs * Gs) - 1.0);
    double k1 = eps_p / eps_s;                           // discrimination factor (< 1)
    if (k1 >= 1.0) k1 = 1.0 - 1e-12;

    // Compute selectivity factor k (modulus of the elliptic prototype) from the
    // degree relation K(k1)/K'(k1) = N · K(k)/K'(k), via the nome:
    //   q1 = exp(-π · K'(k1)/K(k1))
    //   q  = q1^(1/N)
    //   k  = 4·√q · ∏_{n=1..∞} ((1+q^(2n))/(1+q^(2n-1)))⁴
    double K1 = 0.0, K1p = 0.0;
    SpecialFunctions::ellipticIntegralK (k1, K1, K1p);
    double q1 = std::exp (-juce::MathConstants<double>::pi * K1p / K1);
    double q  = std::pow (q1, 1.0 / (double) order);
    double k  = 4.0 * std::sqrt (q);
    {
        double prod = 1.0;
        for (int n = 1; n <= 100; ++n)
        {
            double q2n = std::pow (q, 2.0 * n);
            double q2nm1 = std::pow (q, 2.0 * n - 1.0);
            prod *= (1.0 + q2n) / (1.0 + q2nm1);
            if (q2nm1 < 1e-300) break;
        }
        k *= prod * prod * prod * prod;
    }
    if (k >= 1.0) k = 1.0 - 1e-12;

    int r = order % 2;
    int L = (order - r) / 2;
    Complex<double> j (0.0, 1.0);

    // v_0 (Orfanidis Eq. 16.21). asne returns u in "quarter-period units" (the JUCE
    // convention), so the result is dimensionless and used directly in the cd argument.
    Complex<double> v0 = -j * (SpecialFunctions::asne (j / eps_p, k1) / (double) order);

    // Real pole for odd order
    if (r == 1)
    {
        Complex<double> p0 = j * SpecialFunctions::sne (j * v0, k);
        proto.poles.emplace_back (p0.real(), p0.imag());
    }

    // Complex pole pairs and zero pairs
    for (int i = 1; i <= L; ++i)
    {
        double ui = (2.0 * i - 1.0) / (double) order;

        Complex<double> zeta_i = SpecialFunctions::cde (Complex<double> (ui, 0.0), k);
        Complex<double> p_i    = j * SpecialFunctions::cde (Complex<double> (ui, 0.0) - j * v0, k);
        Complex<double> z_i    = j / (k * zeta_i);

        proto.poles.emplace_back (p_i.real(), p_i.imag());
        proto.poles.emplace_back (p_i.real(), -p_i.imag());
        proto.zeros.emplace_back (z_i.real(), z_i.imag());
        proto.zeros.emplace_back (z_i.real(), -z_i.imag());
    }

    // Initial gain: the prototype above is normalized to passband edge ω_p = 1, with
    // peak passband gain = 1 (Chebyshev's natural max). For the digital cascade
    // normalization step (which scales DC to 1), we want the analog gain factor that
    // makes |H(0)| match the natural prototype value (= 1 odd-order, = 1/√(1+ε²) even).
    Cplx num (1.0, 0.0);
    for (auto& z : proto.zeros) num *= -z;
    Cplx den (1.0, 0.0);
    for (auto& p : proto.poles) den *= -p;
    proto.gain = std::abs (den / num);
    if ((order & 1) == 0)
        proto.gain /= std::sqrt (1.0 + eps_p * eps_p);

    // Rescale poles/zeros so the −3 dB frequency lands at ω = 1 (consistent with
    // every other family in this designer). The −3 dB point lies in the transition
    // band [1, 1/k] when passband ripple < 3 dB, but slips *into* the passband when
    // ripple > 3 dB (since the passband edge gain is then below 1/√2). Pick the
    // bisection interval based on whether the passband edge magnitude is above or
    // below the target — both intervals are monotonic in magnitude.
    {
        const double target = 1.0 / std::sqrt (2.0);
        double mag_at_edge = analogMagnitude (proto, 1.0);
        double w_lo, w_hi;
        if (mag_at_edge >= target)
        {
            w_lo = 1.0;
            w_hi = 1.0 / k;     // transition-band side (ripple ≤ 3 dB)
        }
        else
        {
            // Passband-side: magnitude oscillates inside the ripple band, so step
            // down geometrically from ω=1 until we find one >= target — that's our
            // upper bound. (For typical ripples ≤ 12 dB this happens within a few
            // octaves of the passband edge.)
            w_hi = 1.0;
            w_lo = 0.5;
            int safety = 60;
            while (analogMagnitude (proto, w_lo) < target && safety-- > 0)
                w_lo *= 0.5;
        }
        for (int it = 0; it < 60; ++it)
        {
            double w_mid = std::sqrt (w_lo * w_hi);
            if (analogMagnitude (proto, w_mid) > target) w_lo = w_mid;
            else                                          w_hi = w_mid;
        }
        double w3dB = std::sqrt (w_lo * w_hi);

        for (auto& p : proto.poles) p /= w3dB;
        for (auto& z : proto.zeros) z /= w3dB;
        // After s → s'·w3dB, the leading-coefficient factor changes by w3dB^(M−N).
        // For elliptic LP: M = 2L finite zeros, N = 2L + r poles, so M − N = −r.
        if (r == 1) proto.gain /= w3dB;
    }

    return proto;
}

//==============================================================================
// Frequency transforms (analog domain)
//==============================================================================

namespace
{

// Apply LP→HP transformation s → ωc / s. Each finite pole/zero p maps to ωc/p;
// each zero at infinity becomes a zero at s = 0.
void lowpassToHighpass (Prototype& p, double wc)
{
    for (auto& pole : p.poles) pole = wc / pole;

    int finiteZerosBefore = (int) p.zeros.size();
    for (auto& zero : p.zeros) zero = wc / zero;

    // For each "zero at infinity" in the LP prototype, add a zero at s=0 in the HP prototype.
    int infZerosLP = (int) p.poles.size() - finiteZerosBefore;
    for (int i = 0; i < infZerosLP; ++i)
        p.zeros.emplace_back (0.0, 0.0);

    // Recompute gain factor. After s→ωc/s, the LP H(0) (analog DC gain, equal to the
    // value the prototype was normalized to) becomes the new HP H(∞). For our purposes
    // it's cleaner to renormalize the digital cascade at the end, so just track the
    // multiplicative factor here:
    //   H_HP(s) = H_LP(ωc/s)
    //   leading-coefficient ratio: ∏(−p_LP) / ∏(−p_HP) for poles, etc.
    // We defer exact gain handling to the digital-cascade normalization step.
    (void) wc;  // gain handled later
}

// Scale all poles/zeros so the prototype's normalized cutoff (ω = 1) lands at wc.
void scaleLowpass (Prototype& p, double wc)
{
    for (auto& pole : p.poles) pole *= wc;
    for (auto& zero : p.zeros) zero *= wc;
}

//==============================================================================
// Bilinear transform
//==============================================================================

// Bilinear map z = (2/T + s) / (2/T − s), with K = 2·fs.
inline Cplx bilinearPoint (Cplx s, double K) { return (K + s) / (K - s); }

struct DigitalPolesZeros
{
    std::vector<Cplx> poles;
    std::vector<Cplx> zeros;       // includes finite analog zeros mapped to z, plus zeros at z=-1 from analog ∞ zeros
    int realPoleIndex = -1;        // index into poles[] of the unpaired real pole, or -1
};

DigitalPolesZeros bilinearTransform (const Prototype& proto, double sampleRate)
{
    double K = 2.0 * sampleRate;
    DigitalPolesZeros d;
    d.poles.reserve (proto.poles.size());
    d.zeros.reserve (proto.poles.size());

    for (auto& p : proto.poles) d.poles.push_back (bilinearPoint (p, K));
    for (auto& z : proto.zeros) d.zeros.push_back (bilinearPoint (z, K));

    // Each "zero at infinity" in the analog domain becomes a zero at z = -1.
    int extraZeros = (int) proto.poles.size() - (int) proto.zeros.size();
    for (int i = 0; i < extraZeros; ++i)
        d.zeros.emplace_back (-1.0, 0.0);

    return d;
}

//==============================================================================
// Pole/zero grouping into biquad SOS
//==============================================================================

// Identify the indices of an unpaired real pole (one with imag ≈ 0).
// Returns -1 if none. Treats |im| < tol as real.
int findRealIndex (const std::vector<Cplx>& v, std::vector<bool>& used, double tol = 1e-7)
{
    for (int i = 0; i < (int) v.size(); ++i)
        if (! used[i] && std::abs (v[i].imag()) < tol)
            return i;
    return -1;
}

// Find the index of the conjugate of v[i], not yet used. Returns -1 if not found.
int findConjugate (const std::vector<Cplx>& v, int i, std::vector<bool>& used, double tol = 1e-6)
{
    for (int j = 0; j < (int) v.size(); ++j)
    {
        if (j == i || used[j]) continue;
        if (std::abs (v[j] - std::conj (v[i])) < tol)
            return j;
    }
    return -1;
}

// Build SOS sections by iterating poles (sorted by ascending |Im|) and pairing
// each pole or pole pair with the closest still-unused zero or zero pair.
std::vector<DigitalBiquad>
groupIntoSOS (const DigitalPolesZeros& d)
{
    std::vector<DigitalBiquad> sections;
    auto poles = d.poles;
    auto zeros = d.zeros;

    // Sort by ascending |Im| so high-Q poles end up at the end of the cascade
    // (better numerical behavior in fixed/low-precision arithmetic).
    auto bySortKey = [] (const Cplx& a, const Cplx& b) {
        return std::abs (a.imag()) < std::abs (b.imag());
    };
    std::sort (poles.begin(), poles.end(), bySortKey);
    std::sort (zeros.begin(), zeros.end(), bySortKey);

    std::vector<bool> poleUsed (poles.size(), false);
    std::vector<bool> zeroUsed (zeros.size(), false);

    auto useZeroPair = [&] (Cplx zRef) -> std::pair<double, double> {
        // Find closest unused zero to zRef and (if it's complex) its conjugate.
        int bestIdx = -1;
        double bestDist = std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int) zeros.size(); ++i)
        {
            if (zeroUsed[i]) continue;
            double d2 = std::norm (zeros[i] - zRef);
            if (d2 < bestDist) { bestDist = d2; bestIdx = i; }
        }
        if (bestIdx < 0)
            return { 0.0, 0.0 };           // numerator = 1 (no zero contribution)

        Cplx z = zeros[bestIdx];
        zeroUsed[bestIdx] = true;
        if (std::abs (z.imag()) < 1e-7)
        {
            // Real zero — biquad's numerator absorbs two real zeros to form a
            // proper second-order section. If there's another unused real zero
            // (which is the common case for LP→z=-1 and HP→z=+1 from bilinear
            // of all-pole prototypes), pair them; otherwise fall back to a
            // first-order numerator.
            for (int i = 0; i < (int) zeros.size(); ++i)
            {
                if (zeroUsed[i] || std::abs (zeros[i].imag()) >= 1e-7) continue;
                double z2 = zeros[i].real();
                zeroUsed[i] = true;
                // (1 − z·z⁻¹)(1 − z2·z⁻¹) = 1 − (z + z2)·z⁻¹ + z·z2·z⁻²
                return { -(z.real() + z2), z.real() * z2 };
            }
            return { -z.real(), 0.0 };
        }
        // Complex — find conjugate.
        int cj = findConjugate (zeros, bestIdx, zeroUsed);
        if (cj >= 0) zeroUsed[cj] = true;
        return { -2.0 * z.real(), std::norm (z) };
    };

    auto useSingleZero = [&] () -> double {
        // Used for first-order section: pick a single real zero; if none, return 0
        // (numerator is just 1). Returns the b1 coefficient (numerator = 1 + b1·z⁻¹).
        for (int i = 0; i < (int) zeros.size(); ++i)
        {
            if (zeroUsed[i]) continue;
            if (std::abs (zeros[i].imag()) < 1e-7)
            {
                double zr = zeros[i].real();
                zeroUsed[i] = true;
                return -zr;
            }
        }
        return 0.0;
    };

    // Pair conjugate poles into biquads, leftover real pole into a first-order section.
    for (int i = 0; i < (int) poles.size(); ++i)
    {
        if (poleUsed[i]) continue;
        Cplx p = poles[i];

        if (std::abs (p.imag()) < 1e-7)
        {
            // Real pole: first-order section.
            double pr = p.real();
            double b1 = useSingleZero();
            DigitalBiquad sec;
            sec.b0 = 1.0; sec.b1 = b1; sec.b2 = 0.0;
            sec.a1 = -pr; sec.a2 = 0.0;
            sections.push_back (sec);
            poleUsed[i] = true;
        }
        else
        {
            int cj = findConjugate (poles, i, poleUsed);
            if (cj < 0) continue;          // shouldn't happen for stable real-coeff filters
            poleUsed[i] = true;
            poleUsed[cj] = true;

            auto [b1, b2] = useZeroPair (p);
            DigitalBiquad sec;
            sec.b0 = 1.0; sec.b1 = b1; sec.b2 = b2;
            sec.a1 = -2.0 * p.real();
            sec.a2 = std::norm (p);
            sections.push_back (sec);
        }
    }
    return sections;
}

// Evaluate a biquad cascade's magnitude at digital angular frequency ω
// (where ω = 0 means DC, ω = π means Nyquist).
double evaluateMagnitude (const std::vector<DigitalBiquad>& secs, double omega)
{
    Cplx z (std::cos (omega), std::sin (omega));
    Cplx zInv = 1.0 / z;
    Cplx h (1.0, 0.0);
    for (auto& s : secs)
    {
        Cplx num = (Cplx)s.b0 + s.b1 * zInv + s.b2 * zInv * zInv;
        Cplx den = (Cplx)1.0 + s.a1 * zInv + s.a2 * zInv * zInv;
        h *= num / den;
    }
    return std::abs (h);
}
} // anonymous namespace

std::vector<DigitalBiquad>
designDigitalCascade (Family family, Mode mode, int order,
                       double cutoffHz, double sampleRateHz,
                       double passbandRippleDB, double stopbandAttenDB)
{
    if (order < 1 || sampleRateHz <= 0.0) return {};
    double fNyq = 0.5 * sampleRateHz;
    if (cutoffHz <= 0.0 || cutoffHz >= fNyq) return {};

    // Build normalized analog prototype (cutoff at ω = 1 rad/s).
    Prototype proto;
    switch (family)
    {
        case Family::Butterworth: proto = designButterworth (order); break;
        case Family::Chebyshev1:  proto = designChebyshev1  (order, passbandRippleDB); break;
        case Family::Chebyshev2:  proto = designChebyshev2  (order, stopbandAttenDB);  break;
        case Family::Elliptic:    proto = designElliptic    (order, passbandRippleDB, stopbandAttenDB); break;
        case Family::Bessel:      proto = designBessel      (order); break;
    }
    if (proto.poles.empty()) return {};

    // Pre-warp digital cutoff to analog domain.
    double wAnalog = 2.0 * sampleRateHz * std::tan (kPi * cutoffHz / sampleRateHz);

    // Apply LP→LP scaling or LP→HP transformation.
    if (mode == Mode::Lowpass)
        scaleLowpass (proto, wAnalog);
    else
        lowpassToHighpass (proto, wAnalog);

    // Bilinear transform → digital pole/zero set.
    auto d = bilinearTransform (proto, sampleRateHz);

    // Group into SOS biquads (gain still un-normalized).
    auto sections = groupIntoSOS (d);
    if (sections.empty()) return sections;

    // Normalize overall gain: LP → unity at DC (ω = 0); HP → unity at Nyquist (ω = π).
    double normOmega = (mode == Mode::Lowpass) ? 0.0 : kPi;
    double mag = evaluateMagnitude (sections, normOmega);
    if (mag > 1e-30)
    {
        double scale = 1.0 / mag;
        // For Chebyshev I LP with even order, the natural peak gain (= 1) lies in the ripple
        // band, not at DC, so renormalizing to unity at DC would amplify the ripple.
        // Instead keep the peak gain at 1: undo the renormalization in that case.
        if (family == Family::Chebyshev1 && mode == Mode::Lowpass && (order & 1) == 0)
        {
            double eps = std::sqrt (std::pow (10.0, std::max (0.001, passbandRippleDB) / 10.0) - 1.0);
            scale *= 1.0 / std::sqrt (1.0 + eps * eps); // restore DC-of-ripple amplitude
        }
        // Same situation flipped for Chebyshev I HP with even order: peak is in the ripple band
        // (just above the cutoff), Nyquist sits at the ripple minimum.
        if (family == Family::Chebyshev1 && mode == Mode::Highpass && (order & 1) == 0)
        {
            double eps = std::sqrt (std::pow (10.0, std::max (0.001, passbandRippleDB) / 10.0) - 1.0);
            scale *= 1.0 / std::sqrt (1.0 + eps * eps);
        }
        // Elliptic LP/HP even-order: same passband-ripple convention as Cheby I — DC sits
        // at the ripple minimum, peak gain in the passband is 1.
        if (family == Family::Elliptic && (order & 1) == 0)
        {
            double eps = std::sqrt (std::pow (10.0, std::max (0.001, passbandRippleDB) / 10.0) - 1.0);
            scale *= 1.0 / std::sqrt (1.0 + eps * eps);
        }
        sections.front().b0 *= scale;
        sections.front().b1 *= scale;
        sections.front().b2 *= scale;
    }
    return sections;
}

} // namespace EqDesign
