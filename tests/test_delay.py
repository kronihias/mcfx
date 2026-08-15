"""
mcfx_delay unit tests — Tier 1 (pedalboard, stereo).

mcfx_delay has a single plugin-wide parameter:
  "Delay in ms"  (index 0, applies to ALL channels identically)

Internals (PluginProcessor.cpp):
  delay_samples = floor(param * MAX_DELAYTIME_S * sampleRate + 0.5)   ← rounded
  MAX_DELAYTIME_S = 0.5  (s)  → max 24000 samples at 48 kHz

Important: the mapping back from samples to stored param is:
  _delay_ms = _delay_smpls / sampleRate / MAX_DELAYTIME_S
so the stored value (0–1) is NOT literally milliseconds; it is
a fraction of MAX_DELAYTIME_S.

  delay_ms = param * MAX_DELAYTIME_S * 1000
  param    = delay_ms / (MAX_DELAYTIME_S * 1000)

Tests
-----
1. Zero delay → output == input (same samples, no shift)
2. Known delay values land at the correct sample index (impulse test)
3. Both channels delayed identically
4. Maximum delay (param = 1.0) is honoured
5. Golden regression
"""

import sys, os
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
import pytest

from conftest import SR, BLOCK, save_golden, load_golden, golden_exists

# ---------------------------------------------------------------------------
# mcfx_delay constants (must match PluginProcessor.h)
# ---------------------------------------------------------------------------

MAX_DELAYTIME_S = 0.5   # s — mcfx_delay default


def ms_to_param(delay_ms: float) -> float:
    """Convert milliseconds to normalised [0,1] parameter."""
    return delay_ms / (MAX_DELAYTIME_S * 1000.0)


def param_to_samples(param: float, fs: float = SR) -> int:
    """Mirror plugin's exact formula (rounded)."""
    return int(np.floor(param * MAX_DELAYTIME_S * fs + 0.5))


# ===========================================================================
# Tests
# ===========================================================================

def test_zero_delay(plugin_delay):
    """param = 0 → output identical to input."""
    plugin_delay["Delay in ms"] = 0.0

    rng = np.random.default_rng(1)
    noise = rng.standard_normal((2, BLOCK)).astype(np.float32)
    out   = plugin_delay(noise, SR)

    np.testing.assert_allclose(out, noise, atol=1e-6,
                               err_msg="Zero delay should be transparent")


@pytest.mark.parametrize("delay_ms", [1.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 499.0])
def test_delay_sample_accuracy(plugin_delay, delay_ms):
    """
    Impulse at sample 0 must reappear at exactly the expected sample index.
    """
    param         = ms_to_param(delay_ms)
    expected_smpl = param_to_samples(param, SR)

    plugin_delay["Delay in ms"] = param

    # Need enough samples to see the delayed impulse
    length  = max(BLOCK, expected_smpl + 2048)
    impulse = np.zeros((2, length), dtype=np.float32)
    impulse[0, 0] = 1.0
    impulse[1, 0] = 1.0

    out = plugin_delay(impulse, SR)

    peak_ch0 = int(np.argmax(np.abs(out[0])))
    peak_ch1 = int(np.argmax(np.abs(out[1])))

    assert peak_ch0 == expected_smpl, (
        f"Ch0: delay_ms={delay_ms}, expected sample {expected_smpl}, got {peak_ch0}"
    )
    assert peak_ch1 == expected_smpl, (
        f"Ch1: delay_ms={delay_ms}, expected sample {expected_smpl}, got {peak_ch1}"
    )


def test_both_channels_delayed_equally(plugin_delay):
    """Both channels experience the same delay."""
    param = ms_to_param(20.0)
    plugin_delay["Delay in ms"] = param

    rng  = np.random.default_rng(3)
    # Different signals on each channel
    sig0 = rng.standard_normal(BLOCK).astype(np.float32)
    sig1 = rng.standard_normal(BLOCK).astype(np.float32)
    audio = np.stack([sig0, sig1])

    out = plugin_delay(audio, SR)

    n_delay = param_to_samples(param, SR)
    # Shifted version of each input
    ref0 = np.zeros(BLOCK, dtype=np.float32)
    ref1 = np.zeros(BLOCK, dtype=np.float32)
    ref0[n_delay:] = sig0[:-n_delay]
    ref1[n_delay:] = sig1[:-n_delay]

    np.testing.assert_allclose(out[0, n_delay:], ref0[n_delay:], atol=1e-5)
    np.testing.assert_allclose(out[1, n_delay:], ref1[n_delay:], atol=1e-5)


def test_maximum_delay(plugin_delay):
    """param = 1.0 (maximum) → impulse appears at MAX_DELAYTIME_S * fs samples."""
    plugin_delay["Delay in ms"] = 1.0

    expected_smpl = param_to_samples(1.0, SR)
    length  = expected_smpl + 2048
    impulse = np.zeros((2, length), dtype=np.float32)
    impulse[0, 0] = 1.0

    out      = plugin_delay(impulse, SR)
    peak_idx = int(np.argmax(np.abs(out[0])))

    assert peak_idx == expected_smpl, (
        f"Max delay: expected sample {expected_smpl}, got {peak_idx}"
    )


# ===========================================================================
# Golden regression
# ===========================================================================

GOLDEN_TAG = "delay_10ms_noise"


def test_golden_regression(plugin_delay, request):
    param = ms_to_param(10.0)
    plugin_delay["Delay in ms"] = param

    rng   = np.random.default_rng(99)
    noise = rng.standard_normal((2, BLOCK)).astype(np.float32)
    out   = plugin_delay(noise, SR)

    if request.config.getoption("--update-golden", default=False):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden updated")
    elif not golden_exists(GOLDEN_TAG):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(GOLDEN_TAG)
        np.testing.assert_allclose(out, golden, atol=1e-6)


# ---------------------------------------------------------------------------
# Sample-rate change must not crash (rides in a subprocess: the failure mode
# is a segfault, which would otherwise take the test runner down with it)
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("plugin_name", ["mcfx_delay", "mcfx_gain_delay"])
def test_rate_drop_after_processing_does_not_crash(plugin_name):
    """Process at a high rate, then re-prepare at a low one on the SAME
    instance. The delay ring's write position used to survive prepareToPlay
    while _buf_size shrank beneath it, sending the wrap arithmetic negative —
    a memmove off the end of the buffer (the crash Steinberg's
    ProcessFormatTest, and with it Isadora's plug-in scan, kept hitting)."""
    from conftest import vst3, vst3_load_path
    import subprocess, textwrap

    path = vst3(plugin_name)
    if not os.path.exists(path):
        pytest.skip(f"Plugin not built: {path}")

    script = textwrap.dedent(f"""
        import numpy as np, pedalboard
        p = pedalboard.load_plugin({vst3_load_path(path)!r})
        for k, prm in p.parameters.items():
            if "delay" in k.lower():
                prm.raw_value = 1.0
        # push the ring's write position well past the low-rate buffer size
        p(np.zeros((2, 90112), dtype=np.float32),
          sample_rate=192000.0, buffer_size=512, reset=False)
        p(np.zeros((2, 4096), dtype=np.float32),
          sample_rate=44100.0, buffer_size=512, reset=False)
        print("survived")
    """)
    r = subprocess.run([sys.executable, "-c", script],
                       capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, (
        f"{plugin_name} crashed on sample-rate change "
        f"(exit {r.returncode}): {r.stderr[-400:]}")
    assert "survived" in r.stdout
