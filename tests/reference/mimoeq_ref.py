"""
Reference DSP for mcfx_mimoeq — APVTS parameter conversion.

APVTS NormalisableRange parameters (from PluginProcessor.cpp createParameters()):
  Freq:   NormalisableRange(20, 20000, step=0, skew=0.3)
  Q:      NormalisableRange(0.1, 20,  step=0, skew=0.4)
  Gain:   NormalisableRange(-24, 24,  step=0.01)          — linear dB, no skew
  Enable: AudioParameterBool

JUCE skew mapping (skewFactor != 1):
  convertFrom0to1:  real = start + (end - start) * norm^skew
  convertTo0to1:    norm = ((real - start) / (end - start))^(1/skew)

IIR bands use JUCE IIRCoefficients (Audio EQ Cookbook formulas), identical to
mcfx_filter — see reference/filter_ref.py for the biquad constructors.

EqBand stores gain in dB (via setGainDB) and converts internally:
  linear_gain = 10^(db / 20)    # passed to IIRCoefficients as gain_linear
"""

import numpy as np


# ---------------------------------------------------------------------------
# APVTS parameter ↔ real-value conversion
# ---------------------------------------------------------------------------

def freq_to_norm(f: float) -> float:
    """Frequency (Hz) → normalized [0, 1] for Band X Freq APVTS parameter.

    JUCE NormalisableRange(20, 20000, step=0, skew=0.3) stores the parameter
    internally as  norm = linear_proportion^skew  (forward skew), where
    linear_proportion = (f - 20) / (20000 - 20).  Confirmed empirically from
    pedalboard raw_value (e.g., 186 Hz → 0.2376 = 0.00831^0.3).
    """
    return float(((f - 20.0) / (20000.0 - 20.0)) ** 0.3)


def norm_to_freq(n: float) -> float:
    """Normalized [0, 1] → frequency (Hz).  Inverse of freq_to_norm."""
    return 20.0 + (20000.0 - 20.0) * float(n) ** (1.0 / 0.3)


def q_to_norm(q: float) -> float:
    """Q value → normalized [0, 1] for Band X Q APVTS parameter.

    NormalisableRange(0.1, 20, step=0, skew=0.4): norm = linear_proportion^0.4.
    """
    return float(((q - 0.1) / (20.0 - 0.1)) ** 0.4)


def norm_to_q(n: float) -> float:
    """Normalized [0, 1] → Q value.  Inverse of q_to_norm."""
    return 0.1 + (20.0 - 0.1) * float(n) ** (1.0 / 0.4)


def gain_to_norm(db: float) -> float:
    """dB gain → normalized [0, 1] for Band X Gain APVTS parameter.
    0 dB → 0.5  (midpoint of the ±24 dB range).
    """
    return float((db + 24.0) / 48.0)


def norm_to_gain(n: float) -> float:
    """Normalized [0, 1] → dB gain."""
    return -24.0 + 48.0 * float(n)


def db_to_linear(db: float) -> float:
    """dB → linear amplitude (for IIR coefficient construction via EqBand.setGainDB)."""
    return float(10.0 ** (db / 20.0))
