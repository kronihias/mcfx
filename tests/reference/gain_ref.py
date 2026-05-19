"""
Reference DSP for mcfx_gain_delay — gain and delay channel processing.

From mcfx_gain_delay/Source/PluginProcessor.cpp:

  param2db(param) = param * 36 - 18     → -18 … +18 dB
  dbtorms(db)     = 10^(db/20)          → linear gain
  param2gain(p)   = dbtorms(param2db(p))

  delay_samples   = int(param * MAX_DELAYTIME_S * sampleRate)
  (MAX_DELAYTIME_S defaults to 1.0 s)

  Gain ramp: applyGainRamp over one block when gain changes.
  Once settled, constant gain is applied per sample.
"""

import numpy as np


# ---------------------------------------------------------------------------
# Parameter conversion
# ---------------------------------------------------------------------------

MAX_DELAYTIME_S = 1.0   # matches #define MAX_DELAYTIME_S 1 in PluginProcessor.h


def param2db(p: float) -> float:
    """Normalized [0,1] → dB: p*36 - 18"""
    return p * 36.0 - 18.0


def param2gain(p: float) -> float:
    """Normalized [0,1] → linear amplitude gain"""
    return 10.0 ** (param2db(p) / 20.0)


def gain2param(gain_linear: float) -> float:
    db = 20.0 * np.log10(gain_linear)
    return (db + 18.0) / 36.0


def delay_param_to_samples(p: float, fs: float) -> int:
    """Normalized [0,1] → integer delay in samples."""
    return int(p * MAX_DELAYTIME_S * fs)


def delay_ms_to_param(ms: float, fs: float) -> float:
    """Convenience: ms → normalized param value."""
    return (ms / 1000.0) / MAX_DELAYTIME_S


# ---------------------------------------------------------------------------
# Reference processing
# ---------------------------------------------------------------------------

def apply_gain(audio: np.ndarray, gain_param: float) -> np.ndarray:
    """Apply constant gain (after settling — no ramp)."""
    return (audio * param2gain(gain_param)).astype(np.float32)


def apply_delay(audio: np.ndarray, delay_param: float, fs: float) -> np.ndarray:
    """
    Apply integer sample delay to a 1-D float32 signal.
    Equivalent to prepending zeros and reading from a ring buffer.
    """
    n_delay = delay_param_to_samples(delay_param, fs)
    if n_delay == 0:
        return audio.copy().astype(np.float32)
    out = np.zeros_like(audio)
    out[n_delay:] = audio[:-n_delay]
    return out.astype(np.float32)
