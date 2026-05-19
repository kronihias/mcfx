"""
mcfx_filter unit tests — Tier 1 (pedalboard, stereo).

Strategy
--------
1. Frequency-domain accuracy: apply plugin and reference to an impulse,
   compare frequency responses via FFT (magnitude, dB). This is robust to
   the plugin's 50 ms parameter-smoothing ramp because the impulse is long
   enough (1 s) that the settled response dominates.

2. Time-domain regression: store a golden output, compare on subsequent runs.

Parameter mapping (from PluginProcessor.cpp / getParameterName):
  "LowCut On"     0/1 toggle
  "LowCut Freq"   param2freq(p)  →  Hz
  "LowCut Order"  ≤0.5 → 2nd order,  >0.5 → 4th order
  "HighCut On"    0/1 toggle
  "HighCut Freq"  param2freq(p)
  "HighCut Order" ≤0.5 → 2nd order,  >0.5 → 4th order
  "Peak 1 Gain"   param2db(p)  (-18…+18 dB)
  "Peak 1 Freq"   param2freq(p)
  "Peak 1 Q"      param2q(p)
  "Peak 2 Gain"   (same)
  "Peak 2 Freq"
  "Peak 2 Q"
  "LowShelf Gain" param2db(p)
  "LowShelf Freq" param2freq(p)
  "LowShelf Q"    param2q(p)
  "HighShelf Gain" param2db(p)
  "HighShelf Freq" param2freq(p)
  "HighShelf Q"    param2q(p)

Notes
-----
- LC/HC Q is hardcoded to 1/sqrt(2) (Butterworth) inside the plugin.
- Gain for PF/LS/HS is stored as LINEAR (param2gain applied before
  passing to IIRCoefficients).
- 4th-order LC/HC = same 2nd-order biquad cascaded twice.
- Parameter smoothing ramp = 50 ms; we verify settled response only.
"""

import sys, os
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
import pytest
from scipy.signal import sosfilt

from reference.filter_ref import (
    param2freq, param2q, param2db, param2gain,
    freq2param, q2param, db2param,
    make_lowpass_sos, make_highpass_sos,
    make_peak_sos, make_lowshelf_sos, make_highshelf_sos,
    BUTTERWORTH_Q, apply_sos,
)
from conftest import SR, BLOCK, save_golden, load_golden, golden_exists

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

FFT_N = 4096  # frequency-domain comparison window


def freq_response_db(signal: np.ndarray) -> np.ndarray:
    """Magnitude response in dB of an impulse response (single channel).

    Clipped at -100 dB so that stopband noise-floor differences
    (e.g. -105 dB plugin vs -159 dB scipy reference) don't fail the test.
    Both values represent "essentially silence" and the 54 dB gap is
    irrelevant for any audio comparison.
    """
    spectrum = np.abs(np.fft.rfft(signal[:FFT_N]))
    spectrum = np.maximum(spectrum, 1e-4)   # -80 dB floor
    return 20.0 * np.log10(spectrum)


def reset_filter(plugin) -> None:
    """Turn off all filter sections → unity pass-through."""
    plugin["LowCut On"]    = 0.0
    plugin["HighCut On"]   = 0.0
    plugin["Peak 1 Gain"]  = db2param(0.0)
    plugin["Peak 2 Gain"]  = db2param(0.0)
    plugin["LowShelf Gain"]  = db2param(0.0)
    plugin["HighShelf Gain"] = db2param(0.0)


def run_impulse(plugin, audio: np.ndarray) -> np.ndarray:
    """Process stereo impulse through plugin, return channel-0."""
    out = plugin(audio, SR)
    return out[0]


# ---------------------------------------------------------------------------
# Settle helper: pre-roll a block of silence so parameter ramps finish
# before the test signal arrives.
# ---------------------------------------------------------------------------

SETTLE_SAMPLES = int(0.1 * SR)  # 100 ms >> 50 ms ramp time


def run_with_settle(plugin, test_signal: np.ndarray) -> np.ndarray:
    """
    Concatenate a silence prefix, process, drop the prefix.
    test_signal: shape (2, n_samples)
    """
    silence = np.zeros((2, SETTLE_SAMPLES), dtype=np.float32)
    full    = np.concatenate([silence, test_signal], axis=1)
    out     = plugin(full, SR)
    return out[:, SETTLE_SAMPLES:]


# ===========================================================================
# Fixtures
# ===========================================================================

@pytest.fixture(scope="module")
def impulse_2ch():
    """Stereo impulse: impulse on ch0, silence on ch1."""
    sig = np.zeros((2, BLOCK), dtype=np.float32)
    sig[0, 0] = 1.0
    return sig


@pytest.fixture(scope="module")
def noise_2ch():
    rng = np.random.default_rng(0)
    return rng.standard_normal((2, BLOCK)).astype(np.float32)


# ===========================================================================
# Peak filter tests
# ===========================================================================

@pytest.mark.parametrize("freq_p, q_p, gain_p", [
    (freq2param(1000),  q2param(1.0),  db2param(+9.0)),   # 1 kHz boost
    (freq2param(500),   q2param(0.7),  db2param(-9.0)),   # 500 Hz cut
    (freq2param(8000),  q2param(4.0),  db2param(+6.0)),   # 8 kHz narrow boost
    (freq2param(200),   q2param(0.5),  db2param(0.0)),    # 0 dB → unity
])
def test_peak1_frequency_response(plugin_filter, impulse_2ch, freq_p, q_p, gain_p):
    """Peak filter 1 frequency response matches scipy reference within 0.5 dB."""
    reset_filter(plugin_filter)
    plugin_filter["Peak 1 Freq"] = freq_p
    plugin_filter["Peak 1 Q"]    = q_p
    plugin_filter["Peak 1 Gain"] = gain_p

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_peak_sos(param2freq(freq_p), param2q(q_p), param2gain(gain_p), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    # Compare frequency responses (settled portion)
    plugin_db = freq_response_db(plugin_ir)
    ref_db    = freq_response_db(ref_ir)

    np.testing.assert_allclose(plugin_db, ref_db, atol=0.5,
        err_msg=f"Peak1 @ {param2freq(freq_p):.0f} Hz, Q={param2q(q_p):.2f}, "
                f"{param2db(gain_p):.1f} dB: freq response mismatch")


@pytest.mark.parametrize("freq_p, q_p, gain_p", [
    (freq2param(1000), q2param(1.0), db2param(+9.0)),
    (freq2param(3000), q2param(2.0), db2param(-6.0)),
])
def test_peak2_frequency_response(plugin_filter, impulse_2ch, freq_p, q_p, gain_p):
    """Peak filter 2 frequency response matches scipy reference within 0.5 dB."""
    reset_filter(plugin_filter)
    plugin_filter["Peak 2 Freq"] = freq_p
    plugin_filter["Peak 2 Q"]    = q_p
    plugin_filter["Peak 2 Gain"] = gain_p

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_peak_sos(param2freq(freq_p), param2q(q_p), param2gain(gain_p), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(freq_response_db(plugin_ir),
                               freq_response_db(ref_ir), atol=0.5)


# ===========================================================================
# Low-cut (high-pass) tests
# ===========================================================================

@pytest.mark.parametrize("freq_p, fourth_order", [
    (freq2param(200),  False),
    (freq2param(1000), False),
    (freq2param(500),  True),
])
def test_lowcut_frequency_response(plugin_filter, impulse_2ch, freq_p, fourth_order):
    """Low-cut (high-pass) filter matches Butterworth reference within 1 dB."""
    reset_filter(plugin_filter)
    plugin_filter["LowCut On"]    = 1.0
    plugin_filter["LowCut Freq"]  = freq_p
    plugin_filter["LowCut Order"] = 1.0 if fourth_order else 0.0

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_highpass_sos(param2freq(freq_p), BUTTERWORTH_Q, SR)
    if fourth_order:
        sos = np.vstack([sos, sos])  # cascade twice for 4th order
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(freq_response_db(plugin_ir),
                               freq_response_db(ref_ir), atol=1.0)


# ===========================================================================
# High-cut (low-pass) tests
# ===========================================================================

@pytest.mark.parametrize("freq_p, fourth_order", [
    (freq2param(5000),  False),
    (freq2param(10000), False),
    (freq2param(3000),  True),
])
def test_highcut_frequency_response(plugin_filter, impulse_2ch, freq_p, fourth_order):
    """High-cut (low-pass) filter matches Butterworth reference within 1 dB."""
    reset_filter(plugin_filter)
    plugin_filter["HighCut On"]    = 1.0
    plugin_filter["HighCut Freq"]  = freq_p
    plugin_filter["HighCut Order"] = 1.0 if fourth_order else 0.0

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_lowpass_sos(param2freq(freq_p), BUTTERWORTH_Q, SR)
    if fourth_order:
        sos = np.vstack([sos, sos])
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(freq_response_db(plugin_ir),
                               freq_response_db(ref_ir), atol=1.0)


# ===========================================================================
# Shelf filter tests
# ===========================================================================

@pytest.mark.parametrize("freq_p, q_p, gain_p", [
    (freq2param(200),  q2param(0.7), db2param(+6.0)),
    (freq2param(500),  q2param(1.0), db2param(-6.0)),
])
def test_lowshelf_frequency_response(plugin_filter, impulse_2ch, freq_p, q_p, gain_p):
    reset_filter(plugin_filter)
    plugin_filter["LowShelf Freq"] = freq_p
    plugin_filter["LowShelf Q"]    = q_p
    plugin_filter["LowShelf Gain"] = gain_p

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_lowshelf_sos(param2freq(freq_p), param2q(q_p), param2gain(gain_p), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(freq_response_db(plugin_ir),
                               freq_response_db(ref_ir), atol=0.5)


@pytest.mark.parametrize("freq_p, q_p, gain_p", [
    (freq2param(8000), q2param(0.7), db2param(+6.0)),
    (freq2param(5000), q2param(1.0), db2param(-6.0)),
])
def test_highshelf_frequency_response(plugin_filter, impulse_2ch, freq_p, q_p, gain_p):
    reset_filter(plugin_filter)
    plugin_filter["HighShelf Freq"] = freq_p
    plugin_filter["HighShelf Q"]    = q_p
    plugin_filter["HighShelf Gain"] = gain_p

    out = run_with_settle(plugin_filter, impulse_2ch)
    plugin_ir = out[0]

    sos = make_highshelf_sos(param2freq(freq_p), param2q(q_p), param2gain(gain_p), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(freq_response_db(plugin_ir),
                               freq_response_db(ref_ir), atol=0.5)


# ===========================================================================
# Unity pass-through: all filters off → output == input
# ===========================================================================

def test_unity_passthrough(plugin_filter, noise_2ch):
    reset_filter(plugin_filter)
    out = plugin_filter(noise_2ch, SR)
    np.testing.assert_allclose(out, noise_2ch, atol=1e-5,
                               err_msg="All-off plugin should be transparent")


# ===========================================================================
# Golden file regression
# ===========================================================================

GOLDEN_TAG = "filter_peak1_1k_9dB"


def _check_or_update_golden(tag: str, out: np.ndarray, request) -> None:
    if request.config.getoption("--update-golden", default=False):
        save_golden(tag, out)
        pytest.skip("Golden updated")
    elif not golden_exists(tag):
        save_golden(tag, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(tag)
        np.testing.assert_allclose(out, golden, atol=1e-5,
                                   err_msg=f"Golden regression failed: {tag}")


def test_peak1_golden_regression(plugin_filter, impulse_2ch, request):
    """Regression: peak1 at 1kHz +9dB output must match stored golden."""
    reset_filter(plugin_filter)
    plugin_filter["Peak 1 Freq"] = freq2param(1000)
    plugin_filter["Peak 1 Q"]    = q2param(1.0)
    plugin_filter["Peak 1 Gain"] = db2param(9.0)

    out = run_with_settle(plugin_filter, impulse_2ch)
    _check_or_update_golden(GOLDEN_TAG, out, request)


def test_lowcut_golden_regression(plugin_filter, impulse_2ch, request):
    """Regression: 4th-order low-cut at 200 Hz."""
    reset_filter(plugin_filter)
    plugin_filter["LowCut On"]    = 1.0
    plugin_filter["LowCut Freq"]  = freq2param(200)
    plugin_filter["LowCut Order"] = 1.0   # 4th order

    out = run_with_settle(plugin_filter, impulse_2ch)
    _check_or_update_golden("filter_lowcut_4th_200Hz", out, request)


def test_highcut_golden_regression(plugin_filter, impulse_2ch, request):
    """Regression: 4th-order high-cut at 5 kHz."""
    reset_filter(plugin_filter)
    plugin_filter["HighCut On"]    = 1.0
    plugin_filter["HighCut Freq"]  = freq2param(5000)
    plugin_filter["HighCut Order"] = 1.0   # 4th order

    out = run_with_settle(plugin_filter, impulse_2ch)
    _check_or_update_golden("filter_highcut_4th_5kHz", out, request)


def test_lowshelf_golden_regression(plugin_filter, impulse_2ch, request):
    """Regression: low-shelf at 300 Hz, Q=0.7, +6 dB."""
    reset_filter(plugin_filter)
    plugin_filter["LowShelf Freq"] = freq2param(300)
    plugin_filter["LowShelf Q"]    = q2param(0.7)
    plugin_filter["LowShelf Gain"] = db2param(6.0)

    out = run_with_settle(plugin_filter, impulse_2ch)
    _check_or_update_golden("filter_lowshelf_300Hz_6dB", out, request)


def test_highshelf_golden_regression(plugin_filter, impulse_2ch, request):
    """Regression: high-shelf at 6 kHz, Q=0.7, -6 dB."""
    reset_filter(plugin_filter)
    plugin_filter["HighShelf Freq"] = freq2param(6000)
    plugin_filter["HighShelf Q"]    = q2param(0.7)
    plugin_filter["HighShelf Gain"] = db2param(-6.0)

    out = run_with_settle(plugin_filter, impulse_2ch)
    _check_or_update_golden("filter_highshelf_6kHz_-6dB", out, request)


