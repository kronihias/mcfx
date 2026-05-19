"""
mcfx_gain_delay unit tests — Tier 1 (pedalboard, stereo).

Parameter layout (per-channel, PARAMS_PER_CH = 6):
  index % 6 == 0  →  "Ch N gain"       param ∈ [0,1],  param2db → -18…+18 dB
  index % 6 == 1  →  "Ch N delaytime"  param ∈ [0,1],  samples = p * MAX_DELAYTIME_S * fs
  index % 6 == 2  →  "Ch N phase"      >0.5 → inverted
  index % 6 == 3  →  "Ch N mute"       >0.5 → muted
  index % 6 == 4  →  "Ch N solo"       >0.5 → soloed
  index % 6 == 5  →  "Ch N siggen"     >0.5 → signal generator injected

MAX_DELAYTIME_S = 1 s  (hardcoded default in PluginProcessor.h).

Gain DSP:
  param2db(p) = p * 36 - 18
  linear_gain = 10^(param2db / 20)
  At p = 0.5 → 0 dB → linear_gain = 1.0  (unity).

Delay DSP:
  delay_samples = int(param * 1.0 * sampleRate)   (integer ring-buffer delay)

Tests
-----
1. Unity gain (param=0.5) → output == input
2. Known gain values match param2gain formula
3. Gain on ch0 only, ch1 passes through unchanged
4. Phase invert → output == -input
5. Mute → output == 0
6. Delay: impulse appears at correct sample offset
7. Golden regression
"""

import sys, os
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
import pytest

from reference.gain_ref import (
    param2db, param2gain, gain2param,
    delay_param_to_samples, delay_ms_to_param,
    apply_gain, apply_delay,
)
from conftest import SR, BLOCK, save_golden, load_golden, golden_exists

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

SETTLE_SAMPLES = int(0.25 * SR)  # 250 ms pre-roll — gain/mute/phase crossfade takes ~170 ms


def run_with_settle(plugin, test_signal: np.ndarray) -> np.ndarray:
    silence = np.zeros((test_signal.shape[0], SETTLE_SAMPLES), dtype=np.float32)
    full    = np.concatenate([silence, test_signal], axis=1)
    out     = plugin(full, SR)
    return out[:, SETTLE_SAMPLES:]


def set_unity(plugin, n_ch: int = 2) -> None:
    """Reset all per-channel params to unity (0 dB, no delay, no invert, no mute)."""
    for ch in range(1, n_ch + 1):
        plugin[f"Ch {ch} gain"]      = gain2param(1.0)   # 0 dB
        plugin[f"Ch {ch} delaytime"] = 0.0
        plugin[f"Ch {ch} phase"]     = 0.0
        plugin[f"Ch {ch} mute"]      = 0.0
        plugin[f"Ch {ch} solo"]      = 0.0
        plugin[f"Ch {ch} signalgenerator"] = 0.0


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def noise_2ch():
    rng = np.random.default_rng(7)
    return rng.standard_normal((2, BLOCK)).astype(np.float32)


@pytest.fixture(scope="module")
def impulse_2ch():
    sig = np.zeros((2, BLOCK), dtype=np.float32)
    sig[0, 0] = 1.0
    sig[1, 0] = 1.0
    return sig


# ===========================================================================
# Gain tests
# ===========================================================================

def test_unity_gain_passthrough(plugin_gain_delay, noise_2ch):
    """param=0.5 (0 dB) → output identical to input."""
    set_unity(plugin_gain_delay)
    out = run_with_settle(plugin_gain_delay, noise_2ch)
    np.testing.assert_allclose(out, noise_2ch, atol=1e-5,
                               err_msg="Unity gain should be transparent")


@pytest.mark.parametrize("gain_param, expected_db", [
    (gain2param(2.0),  6.0206),   # +6 dB
    (gain2param(0.5), -6.0206),   # -6 dB
    (1.0,             18.0),      # param=1 → +18 dB
    (0.0,            -18.0),      # param=0 → -18 dB
])
def test_gain_level(plugin_gain_delay, noise_2ch, gain_param, expected_db):
    """Gain-applied RMS matches expected dB shift relative to input RMS."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 gain"] = gain_param

    out = run_with_settle(plugin_gain_delay, noise_2ch)

    # Measure RMS ratio ch0 out vs ch0 in (settled)
    rms_in  = np.sqrt(np.mean(noise_2ch[0] ** 2))
    rms_out = np.sqrt(np.mean(out[0] ** 2))
    measured_db = 20.0 * np.log10(rms_out / rms_in)

    assert abs(measured_db - expected_db) < 0.2, (
        f"Expected {expected_db:.2f} dB, got {measured_db:.2f} dB"
    )


def test_gain_per_channel_independence(plugin_gain_delay, noise_2ch):
    """Gain on ch1 does not affect ch2."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 gain"] = gain2param(4.0)   # +12 dB on ch1
    plugin_gain_delay["Ch 2 gain"] = gain2param(1.0)   # unity on ch2

    out = run_with_settle(plugin_gain_delay, noise_2ch)

    np.testing.assert_allclose(out[1], noise_2ch[1], atol=1e-4,
                               err_msg="Ch2 should be unaffected by ch1 gain change")


# ===========================================================================
# Phase invert
# ===========================================================================

def test_phase_invert_ch1(plugin_gain_delay, noise_2ch):
    """Phase invert on ch1 → output[ch0] == -input[ch0]."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 phase"] = 1.0   # >0.5 → invert

    out = run_with_settle(plugin_gain_delay, noise_2ch)
    np.testing.assert_allclose(out[0], -noise_2ch[0], atol=1e-5,
                               err_msg="Phase-inverted ch should equal negated input")


# ===========================================================================
# Mute
# ===========================================================================

def test_mute_ch1(plugin_gain_delay, noise_2ch):
    """Muted channel → output is silence."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 mute"] = 1.0

    out = run_with_settle(plugin_gain_delay, noise_2ch)
    np.testing.assert_allclose(out[0], 0.0, atol=1e-6,
                               err_msg="Muted ch should produce silence")


def test_mute_only_affects_target_channel(plugin_gain_delay, noise_2ch):
    """Muting ch1 leaves ch2 untouched."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 mute"] = 1.0

    out = run_with_settle(plugin_gain_delay, noise_2ch)
    np.testing.assert_allclose(out[1], noise_2ch[1], atol=1e-5)


# ===========================================================================
# Delay tests
# ===========================================================================

@pytest.mark.parametrize("delay_ms", [0.0, 5.0, 10.0, 50.0, 100.0, 500.0])
def test_delay_sample_accuracy(plugin_gain_delay, delay_ms):
    """Integer ring-buffer delay: impulse appears at correct sample offset."""
    delay_param   = delay_ms_to_param(delay_ms, SR)
    expected_smpl = delay_param_to_samples(delay_param, SR)

    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 delaytime"] = delay_param

    # Impulse padded with enough silence to capture delayed output
    length  = max(BLOCK, expected_smpl + 1024)
    impulse = np.zeros((2, length), dtype=np.float32)
    impulse[0, 0] = 1.0

    out = plugin_gain_delay(impulse, SR)

    # Find the peak on ch0 — should land exactly at expected_smpl
    peak_idx = int(np.argmax(np.abs(out[0])))
    assert peak_idx == expected_smpl, (
        f"delay_ms={delay_ms}: expected peak at sample {expected_smpl}, "
        f"got {peak_idx}"
    )


def test_delay_ch2_unaffected(plugin_gain_delay, noise_2ch):
    """Delay on ch1 does not affect ch2."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 delaytime"] = delay_ms_to_param(50.0, SR)

    out = run_with_settle(plugin_gain_delay, noise_2ch)
    # ch2 has no delay → should match input (after settle)
    np.testing.assert_allclose(out[1], noise_2ch[1], atol=1e-4)


# ===========================================================================
# Solo
# ===========================================================================

def test_solo_ch1_silences_ch2(plugin_gain_delay, noise_2ch):
    """Soloing ch1 silences every non-soloed channel; ch1 itself unchanged."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 solo"] = 1.0

    out = run_with_settle(plugin_gain_delay, noise_2ch)

    np.testing.assert_allclose(out[0], noise_2ch[0], atol=1e-5,
        err_msg="Soloed channel should pass through unchanged")
    np.testing.assert_allclose(out[1], 0.0, atol=1e-6,
        err_msg="Non-soloed channel should be silenced when another ch is solo")


# ===========================================================================
# Signal generator
# ===========================================================================
#
# Global params (see mcfx_gain_delay/Source/MySignalGenerator.cpp):
#   SigGenGain        : jmap(param, -99, +6) dB        →  param ≈ 0.943 ⇒ 0 dB
#   SigGenSignaltype  : floor(param * 6) ∈ {white, pink, sine, sawtooth,
#                       square, dirac, toneburst}      →  param = 2/6 ⇒ sine
#   SigGen Freq       : 2^(param*11.07 + 3.33) Hz      →  param ≈ 0.600 ⇒ 1 kHz
# Per-channel: "Ch N signalgenerator" >0.5 → SUMMED into ch N (input is not muted)

def siggen_gain_param_for_db(db: float) -> float:
    return (db - (-99.0)) / (6.0 - (-99.0))


def siggen_freq_param_for_hz(hz: float) -> float:
    return (np.log2(hz) - 3.33) / 11.07


SIGGEN_TYPE_SINE = 2.0 / 6.0  # white=0, pink=1, sine=2, sawtooth=3, ...


def test_signal_generator_sine_appears_on_target_channel(plugin_gain_delay):
    """Enabling siggen on ch1 with a known sine frequency produces a peak at
    that frequency in ch1's output; ch2 stays silent (no siggen, silent input)."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["SigGenSignaltype"]   = SIGGEN_TYPE_SINE
    plugin_gain_delay["SigGenGain"]         = siggen_gain_param_for_db(0.0)
    plugin_gain_delay["SigGen Freq"]        = siggen_freq_param_for_hz(1000.0)
    plugin_gain_delay["Ch 1 signalgenerator"] = 1.0

    silence = np.zeros((2, BLOCK), dtype=np.float32)
    out = run_with_settle(plugin_gain_delay, silence)

    # FFT ch0 — only enough samples to get a clean peak (use power-of-two)
    fft_n = 16384
    spectrum = np.abs(np.fft.rfft(out[0, :fft_n]))
    peak_bin = int(np.argmax(spectrum))
    peak_hz  = peak_bin * SR / fft_n

    assert abs(peak_hz - 1000.0) < SR / fft_n + 1.0, (
        f"Expected siggen peak near 1000 Hz, got {peak_hz:.1f} Hz "
        f"(bin resolution {SR/fft_n:.1f} Hz)"
    )
    # Untargeted channel: input was silence, siggen is off → output silence.
    np.testing.assert_allclose(out[1], 0.0, atol=1e-5,
        err_msg="Non-targeted channel should remain silent")


# ===========================================================================
# Golden regression
# ===========================================================================

GOLDEN_TAG = "gain_delay_ch1_6dB_10ms"


def test_golden_regression(plugin_gain_delay, noise_2ch, request):
    """Regression: ch1 +6 dB, 10 ms delay — output must match stored golden."""
    set_unity(plugin_gain_delay)
    plugin_gain_delay["Ch 1 gain"]      = gain2param(2.0)           # +6 dB
    plugin_gain_delay["Ch 1 delaytime"] = delay_ms_to_param(10.0, SR)

    out = run_with_settle(plugin_gain_delay, noise_2ch)

    if request.config.getoption("--update-golden", default=False):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden updated")
    elif not golden_exists(GOLDEN_TAG):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(GOLDEN_TAG)
        np.testing.assert_allclose(out, golden, atol=1e-5)
