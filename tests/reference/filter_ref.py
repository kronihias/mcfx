"""
Reference DSP for mcfx_filter — matches JUCE IIRCoefficients exactly.

Parameter conversion formulas from:
  mcfx_filter/Source/PluginProcessor.h

Filter coefficients use JUCE's IIRCoefficients Audio EQ Cookbook formulas:
  makeLowPass, makeHighPass, makePeakFilter, makeLowShelf, makeHighShelf

All biquad sections returned as scipy 'sos' row: [b0 b1 b2 1 a1 a2]
"""

import numpy as np
from scipy.signal import sosfilt


# ---------------------------------------------------------------------------
# Parameter conversion (matches PluginProcessor.h exactly)
# ---------------------------------------------------------------------------

def param2freq(p: float) -> float:
    """Normalized [0,1] → frequency in Hz: 2^(p*9.8 + 4.6) ≈ 24 … 21618 Hz"""
    return 2.0 ** (p * 9.8 + 4.6)


def freq2param(freq: float) -> float:
    return (np.log2(freq) - 4.6) / 9.8


def param2q(p: float) -> float:
    """Normalized [0,1] → Q: 2^(p*6.6439) * 0.2 ≈ 0.2 … 20"""
    return 2.0 ** (p * 6.6439) * 0.2


def q2param(q: float) -> float:
    return np.log2(q / 0.2) / 6.6439


def param2db(p: float) -> float:
    """Normalized [0,1] → gain in dB: p*36 - 18 ≈ -18 … +18 dB"""
    return p * 36.0 - 18.0


def db2param(db: float) -> float:
    return (db + 18.0) / 36.0


def param2gain(p: float) -> float:
    """Normalized [0,1] → linear gain (used as IIR gain parameter)"""
    return 10.0 ** (param2db(p) / 20.0)


# ---------------------------------------------------------------------------
# Biquad constructors matching JUCE IIRCoefficients (Audio EQ Cookbook)
# ---------------------------------------------------------------------------
# All return a 1×6 numpy array suitable for scipy.signal.sosfilt:
#   [b0, b1, b2, 1.0, a1, a2]   (a0 normalised to 1)

def make_lowpass_sos(f0: float, Q: float, fs: float) -> np.ndarray:
    """JUCE IIRCoefficients::makeLowPass"""
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * Q)
    cos_w0 = np.cos(w0)
    b0 = (1.0 - cos_w0) / 2.0
    b1 = 1.0 - cos_w0
    b2 = (1.0 - cos_w0) / 2.0
    a0 = 1.0 + alpha
    a1 = -2.0 * cos_w0
    a2 = 1.0 - alpha
    return np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])


def make_highpass_sos(f0: float, Q: float, fs: float) -> np.ndarray:
    """JUCE IIRCoefficients::makeHighPass"""
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * Q)
    cos_w0 = np.cos(w0)
    b0 = (1.0 + cos_w0) / 2.0
    b1 = -(1.0 + cos_w0)
    b2 = (1.0 + cos_w0) / 2.0
    a0 = 1.0 + alpha
    a1 = -2.0 * cos_w0
    a2 = 1.0 - alpha
    return np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])


def make_peak_sos(f0: float, Q: float, gain_linear: float, fs: float) -> np.ndarray:
    """JUCE IIRCoefficients::makePeakFilter  (gain is LINEAR, not dB)"""
    A = np.sqrt(gain_linear)  # JUCE uses A = sqrt(gain)
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * Q)
    cos_w0 = np.cos(w0)
    b0 = 1.0 + alpha * A
    b1 = -2.0 * cos_w0
    b2 = 1.0 - alpha * A
    a0 = 1.0 + alpha / A
    a1 = -2.0 * cos_w0
    a2 = 1.0 - alpha / A
    return np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])


def make_lowshelf_sos(f0: float, Q: float, gain_linear: float, fs: float) -> np.ndarray:
    """JUCE IIRCoefficients::makeLowShelf  (gain is LINEAR, not dB)"""
    A = np.sqrt(gain_linear)
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * Q)
    cos_w0 = np.cos(w0)
    b0 = A * ((A + 1) - (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha)
    b1 = 2 * A * ((A - 1) - (A + 1) * cos_w0)
    b2 = A * ((A + 1) - (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha)
    a0 = (A + 1) + (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha
    a1 = -2 * ((A - 1) + (A + 1) * cos_w0)
    a2 = (A + 1) + (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha
    return np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])


def make_highshelf_sos(f0: float, Q: float, gain_linear: float, fs: float) -> np.ndarray:
    """JUCE IIRCoefficients::makeHighShelf  (gain is LINEAR, not dB)"""
    A = np.sqrt(gain_linear)
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * Q)
    cos_w0 = np.cos(w0)
    b0 = A * ((A + 1) + (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha)
    b1 = -2 * A * ((A - 1) + (A + 1) * cos_w0)
    b2 = A * ((A + 1) + (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha)
    a0 = (A + 1) - (A - 1) * cos_w0 + 2 * np.sqrt(A) * alpha
    a1 = 2 * ((A - 1) - (A + 1) * cos_w0)
    a2 = (A + 1) - (A - 1) * cos_w0 - 2 * np.sqrt(A) * alpha
    return np.array([[b0/a0, b1/a0, b2/a0, 1.0, a1/a0, a2/a0]])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

BUTTERWORTH_Q = 1.0 / np.sqrt(2.0)  # Q used by LC/HC in mcfx_filter


def apply_sos(sos: np.ndarray, audio: np.ndarray) -> np.ndarray:
    """Apply sos biquad cascade to float32 audio, return float32."""
    return sosfilt(sos, audio).astype(np.float32)
