"""
mcfx_mimoeq comprehensive tests.

Strategy
--------

Tier-1 (pedalboard, stereo):
  Configure the diagonal chain via APVTS parameters (Band X Freq/Q/Gain/Enable).
  Compare impulse-response frequency spectra against scipy reference filters.
  Default factory preset (6 bands): HP(off), LowShelf, Peak, Peak, HighShelf, LP(off).

Tier-2 (mcfx_testhost CLI, multichannel):
  Load a full JSON config via the "Mimoeq Config File" testhost key.
  Tests cover: Gain bands, Delay bands, FIR bands, MIMO cross-channel routing,
  diagonal channel masking, and golden-file regressions.
  Requires: cmake -B _build -DBUILD_VST3=ON -DBUILD_TESTHOST=ON
            cmake --build _build --target mcfx_testhost mcfx_mimoeq_VST3
            pip install soundfile

APVTS parameter name format: "Band {N} Freq|Q|Gain|Enable"  (display name)
  Freq:   NormalisableRange(20, 20000, skew=0.3) — freq_to_norm() in mimoeq_ref
  Q:      NormalisableRange(0.1, 20,   skew=0.4) — q_to_norm()
  Gain:   NormalisableRange(-24, 24)             — gain_to_norm(), 0 dB → 0.5
  Enable: bool, raw_value 0.0 = off, 1.0 = on

Default diagonal chain (factory preset):
  Band 1: HighPass   48 Hz,   Q=0.707 — DISABLED by default
  Band 2: LowShelf   94 Hz,   Q=0.707, gain=0 dB
  Band 3: Peak      186 Hz,   Q=0.707, gain=0 dB
  Band 4: Peak     1428 Hz,   Q=0.707, gain=0 dB
  Band 5: HighShelf 2817 Hz,  Q=0.707, gain=0 dB
  Band 6: LowPass  10960 Hz,  Q=0.707 — DISABLED by default

IIR coefficients match JUCE IIRCoefficients (Audio EQ Cookbook) — same as
mcfx_filter, see reference/filter_ref.py for the biquad constructors.
"""

import sys, os, json, tempfile
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
import pytest
from scipy.signal import sosfilt

from reference.filter_ref import (
    make_lowpass_sos, make_highpass_sos,
    make_peak_sos, make_lowshelf_sos, make_highshelf_sos,
    apply_sos,
)
from reference.mimoeq_ref import (
    freq_to_norm, q_to_norm, gain_to_norm,
    norm_to_freq, db_to_linear,
)
from conftest import SR, BLOCK, save_golden, load_golden, golden_exists, run_testhost

TESTHOST_BIN = os.path.join(
    os.path.dirname(os.path.dirname(__file__)),
    "_build", "testhost", "mcfx_testhost"
)

# ---------------------------------------------------------------------------
# Tier-1 fixtures (pedalboard)
# ---------------------------------------------------------------------------

def _load(name: str):
    try:
        import pedalboard
    except ImportError:
        pytest.skip("pedalboard not installed — run: pip install pedalboard")
    from conftest import vst3, PluginWrapper
    path = vst3(name)
    if not os.path.exists(path):
        pytest.skip(f"Plugin not built: {path}")
    return PluginWrapper(pedalboard.load_plugin(path))


@pytest.fixture(scope="module")
def plugin_mimoeq():
    return _load("mcfx_mimoeq")


@pytest.fixture(scope="module")
def impulse_2ch():
    """Stereo signal: impulse on ch0, silence on ch1."""
    sig = np.zeros((2, BLOCK), dtype=np.float32)
    sig[0, 0] = 1.0
    return sig


@pytest.fixture(scope="module")
def noise_2ch():
    rng = np.random.default_rng(17)
    return rng.standard_normal((2, BLOCK)).astype(np.float32)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

FFT_N = 4096  # frequency-domain comparison window
SETTLE_SAMPLES = int(0.1 * SR)  # 100 ms > 50 ms parameter-smoothing ramp


def freq_response_db(signal: np.ndarray) -> np.ndarray:
    """Magnitude response in dB of an impulse response.
    Clipped at -80 dB so stopband noise-floor differences don't fail tests."""
    spectrum = np.abs(np.fft.rfft(signal[:FFT_N]))
    spectrum = np.maximum(spectrum, 1e-4)  # -80 dB floor
    return 20.0 * np.log10(spectrum)


def run_with_settle(plugin, signal: np.ndarray) -> np.ndarray:
    """Prepend silence so parameter ramps settle before the test signal.
    signal: (2, n_samples) float32
    Returns: (2, n_samples) trimmed to original length.
    """
    silence = np.zeros((2, SETTLE_SAMPLES), dtype=np.float32)
    full = np.concatenate([silence, signal], axis=1)
    out = plugin(full, SR)
    return out[:, SETTLE_SAMPLES:]


# Factory preset default parameters (for reset_mimoeq)
_DEFAULTS = [
    (48.0,   0.707, False),   # Band 1: HighPass,  disabled
    (94.0,   0.707, True),    # Band 2: LowShelf
    (186.0,  0.707, True),    # Band 3: Peak
    (1428.0, 0.707, True),    # Band 4: Peak
    (2817.0, 0.707, True),    # Band 5: HighShelf
    (10960.0, 0.707, False),  # Band 6: LowPass,   disabled
]


def reset_mimoeq(plugin) -> None:
    """Reset all diagonal bands to factory defaults (unity passthrough)."""
    for i, (f, q, en) in enumerate(_DEFAULTS, start=1):
        plugin[f"Band {i} Freq"]   = freq_to_norm(f)
        plugin[f"Band {i} Q"]      = q_to_norm(q)
        plugin[f"Band {i} Gain"]   = gain_to_norm(0.0)   # 0 dB = 0.5 normalized
        plugin[f"Band {i} Enable"] = 1.0 if en else 0.0


# ---------------------------------------------------------------------------
# Tier-2 helper: run testhost with a JSON config file
# ---------------------------------------------------------------------------

def run_mimoeq_json(config: dict, audio: np.ndarray,
                   n_channels: int = 2) -> np.ndarray:
    """
    Write *config* to a temp JSON file, then run mcfx_mimoeq via testhost
    with "Mimoeq Config File" pointing to it.

    Requires:
      - BUILD_TESTHOST=ON in cmake
      - testhost built with "Mimoeq Config File" key support (Main.cpp)
    """
    if not os.path.exists(TESTHOST_BIN):
        pytest.skip(f"mcfx_testhost not built: {TESTHOST_BIN}")

    # Write config to a named temp file that outlives the subprocess call
    fd, cfg_path = tempfile.mkstemp(suffix=".json", prefix="mimoeq_cfg_")
    try:
        with os.fdopen(fd, "w") as fp:
            json.dump(config, fp)
        params = {"Mimoeq Config File": cfg_path}
        return run_testhost("mcfx_mimoeq", params, audio, SR)
    finally:
        if os.path.exists(cfg_path):
            os.unlink(cfg_path)


# ===========================================================================
# Tier-1 — Unity passthrough
# ===========================================================================

def test_unity_passthrough(plugin_mimoeq, noise_2ch):
    """Default preset with all gains at 0 dB and HP/LP disabled → output == input."""
    reset_mimoeq(plugin_mimoeq)
    out = plugin_mimoeq(noise_2ch, SR)
    np.testing.assert_allclose(
        out, noise_2ch, atol=1e-5,
        err_msg="All-neutral mimoeq should be transparent"
    )


# ===========================================================================
# Tier-1 — Peak filter (Band 3, factory type = Peak)
# ===========================================================================

@pytest.mark.parametrize("f, q, db", [
    (1000.0, 1.0,  +9.0),   # 1 kHz boost
    (500.0,  0.7,  -9.0),   # 500 Hz cut
    (8000.0, 4.0,  +6.0),   # 8 kHz narrow boost
    (200.0,  0.5,   0.0),   # 0 dB → unity
])
def test_peak_diagonal_freq_response(plugin_mimoeq, impulse_2ch, f, q, db):
    """Peak filter (Band 3) frequency response matches scipy within 0.5 dB."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 3 Freq"] = freq_to_norm(f)
    plugin_mimoeq["Band 3 Q"]    = q_to_norm(q)
    plugin_mimoeq["Band 3 Gain"] = gain_to_norm(db)

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    sos = make_peak_sos(f, q, db_to_linear(db), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=0.5,
        err_msg=f"Peak @ {f:.0f} Hz, Q={q}, {db:+.1f} dB"
    )


# ===========================================================================
# Tier-1 — Low-shelf filter (Band 2, factory type = LowShelf)
# ===========================================================================

@pytest.mark.parametrize("f, q, db", [
    (200.0, 0.7, +6.0),
    (500.0, 1.0, -6.0),
])
def test_lowshelf_diagonal_freq_response(plugin_mimoeq, impulse_2ch, f, q, db):
    """Low-shelf (Band 2) frequency response matches scipy within 0.5 dB."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 2 Freq"] = freq_to_norm(f)
    plugin_mimoeq["Band 2 Q"]    = q_to_norm(q)
    plugin_mimoeq["Band 2 Gain"] = gain_to_norm(db)

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    sos = make_lowshelf_sos(f, q, db_to_linear(db), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=0.5,
        err_msg=f"LowShelf @ {f:.0f} Hz, Q={q}, {db:+.1f} dB"
    )


# ===========================================================================
# Tier-1 — High-shelf filter (Band 5, factory type = HighShelf)
# ===========================================================================

@pytest.mark.parametrize("f, q, db", [
    (8000.0, 0.7, +6.0),
    (5000.0, 1.0, -6.0),
])
def test_highshelf_diagonal_freq_response(plugin_mimoeq, impulse_2ch, f, q, db):
    """High-shelf (Band 5) frequency response matches scipy within 0.5 dB."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 5 Freq"] = freq_to_norm(f)
    plugin_mimoeq["Band 5 Q"]    = q_to_norm(q)
    plugin_mimoeq["Band 5 Gain"] = gain_to_norm(db)

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    sos = make_highshelf_sos(f, q, db_to_linear(db), SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=0.5,
        err_msg=f"HighShelf @ {f:.0f} Hz, Q={q}, {db:+.1f} dB"
    )


# ===========================================================================
# Tier-1 — High-pass filter (Band 1, factory type = HighPass; default disabled)
# ===========================================================================

@pytest.mark.parametrize("f, q", [
    (200.0,  0.707),
    (1000.0, 0.707),
    (500.0,  1.0),
])
def test_highpass_diagonal(plugin_mimoeq, impulse_2ch, f, q):
    """High-pass (Band 1) frequency response matches scipy Butterworth within 1 dB."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 1 Freq"]   = freq_to_norm(f)
    plugin_mimoeq["Band 1 Q"]      = q_to_norm(q)
    plugin_mimoeq["Band 1 Enable"] = 1.0   # enable HP

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    sos = make_highpass_sos(f, q, SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=1.0,
        err_msg=f"HighPass @ {f:.0f} Hz, Q={q}"
    )


# ===========================================================================
# Tier-1 — Low-pass filter (Band 6, factory type = LowPass; default disabled)
# ===========================================================================

@pytest.mark.parametrize("f, q", [
    (5000.0,  0.707),
    (10000.0, 0.707),
    (3000.0,  1.0),
])
def test_lowpass_diagonal(plugin_mimoeq, impulse_2ch, f, q):
    """Low-pass (Band 6) frequency response matches scipy Butterworth within 1 dB."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 6 Freq"]   = freq_to_norm(f)
    plugin_mimoeq["Band 6 Q"]      = q_to_norm(q)
    plugin_mimoeq["Band 6 Enable"] = 1.0   # enable LP

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    sos = make_lowpass_sos(f, q, SR)
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=1.0,
        err_msg=f"LowPass @ {f:.0f} Hz, Q={q}"
    )


# ===========================================================================
# Tier-1 — Multiple bands in series
# ===========================================================================

def test_multiple_bands_serial(plugin_mimoeq, impulse_2ch):
    """Two active diagonal bands (LowShelf + Peak) are processed serially."""
    reset_mimoeq(plugin_mimoeq)

    # Band 2: LowShelf at 200 Hz, +6 dB
    plugin_mimoeq["Band 2 Freq"] = freq_to_norm(200.0)
    plugin_mimoeq["Band 2 Q"]    = q_to_norm(0.7)
    plugin_mimoeq["Band 2 Gain"] = gain_to_norm(+6.0)

    # Band 3: Peak at 1 kHz, -6 dB
    plugin_mimoeq["Band 3 Freq"] = freq_to_norm(1000.0)
    plugin_mimoeq["Band 3 Q"]    = q_to_norm(1.0)
    plugin_mimoeq["Band 3 Gain"] = gain_to_norm(-6.0)

    out = run_with_settle(plugin_mimoeq, impulse_2ch)
    plugin_ir = out[0]

    # Reference: serial cascade (same order as plugin)
    sos_shelf = make_lowshelf_sos(200.0, 0.7, db_to_linear(+6.0), SR)
    sos_peak  = make_peak_sos(1000.0, 1.0,  db_to_linear(-6.0), SR)
    sos = np.vstack([sos_shelf, sos_peak])
    ref_ir = apply_sos(sos, impulse_2ch[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=0.5,
        err_msg="Serial LowShelf+Peak: frequency response mismatch"
    )


# ===========================================================================
# Tier-1 — Golden regression
# ===========================================================================

GOLDEN_DIAG_TAG = "mimoeq_diag_peak_shelf"


def test_golden_regression_diagonal(plugin_mimoeq, impulse_2ch, request):
    """Regression: peak+shelf configuration output must match stored golden."""
    reset_mimoeq(plugin_mimoeq)
    plugin_mimoeq["Band 3 Freq"] = freq_to_norm(1000.0)
    plugin_mimoeq["Band 3 Q"]    = q_to_norm(1.5)
    plugin_mimoeq["Band 3 Gain"] = gain_to_norm(+6.0)
    plugin_mimoeq["Band 5 Freq"] = freq_to_norm(8000.0)
    plugin_mimoeq["Band 5 Q"]    = q_to_norm(0.707)
    plugin_mimoeq["Band 5 Gain"] = gain_to_norm(-3.0)

    out = run_with_settle(plugin_mimoeq, impulse_2ch)

    try:
        update = request.config.getoption("--update-golden")
    except ValueError:
        update = False

    if update:
        save_golden(GOLDEN_DIAG_TAG, out)
        pytest.skip("Golden updated")
    elif not golden_exists(GOLDEN_DIAG_TAG):
        save_golden(GOLDEN_DIAG_TAG, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(GOLDEN_DIAG_TAG)
        np.testing.assert_allclose(out, golden, atol=1e-5,
                                   err_msg="Diagonal golden regression failed")


# ===========================================================================
# Tier-2 — Gain band (diagonal)
# ===========================================================================

@pytest.mark.parametrize("gain_db, expected_linear", [
    (-6.0,  10 ** (-6.0 / 20)),
    (+6.0,  10 ** (+6.0 / 20)),
    (-20.0, 10 ** (-20.0 / 20)),
    (  0.0, 1.0),
])
def test_gain_band_diagonal(gain_db, expected_linear):
    """Diagonal Gain band applies correct linear amplitude scaling."""
    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {"type": "gain", "gain_db": gain_db}}
        ]
    }
    # Constant signal on both channels to make amplitude measurement simple
    audio = np.full((2, BLOCK), 0.5, dtype=np.float32)
    out = run_mimoeq_json(config, audio)

    expected = 0.5 * expected_linear
    # Skip first block in case of any startup transient
    np.testing.assert_allclose(
        out[0, 512:], expected, atol=1e-4,
        err_msg=f"Gain band {gain_db:+.1f} dB: amplitude mismatch"
    )
    np.testing.assert_allclose(
        out[1, 512:], expected, atol=1e-4,
        err_msg=f"Gain band {gain_db:+.1f} dB: ch2 amplitude mismatch"
    )


# ===========================================================================
# Tier-2 — Delay band (diagonal)
# ===========================================================================

@pytest.mark.parametrize("delay_samples", [48, 128, 512])
def test_delay_band_diagonal(delay_samples):
    """Diagonal Delay band shifts the signal by exactly delay_samples."""
    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {
                "type": "delay", "delay_samples": delay_samples
            }}
        ]
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0   # impulse on both channels

    out = run_mimoeq_json(config, audio)

    # Peak should appear exactly at offset delay_samples
    np.testing.assert_allclose(
        out[0, delay_samples], 1.0, atol=1e-5,
        err_msg=f"Delay {delay_samples} samples: peak at wrong position"
    )
    np.testing.assert_allclose(
        out[0, :delay_samples], 0.0, atol=1e-5,
        err_msg=f"Delay {delay_samples} samples: pre-delay samples non-zero"
    )


# ===========================================================================
# Tier-2 — FIR band (diagonal, short filter — direct-form, zero-latency)
# ===========================================================================

def test_fir_band_diagonal_short():
    """Short diagonal FIR (≤128 taps, direct-form) output matches scipy.fftconvolve."""
    from scipy.signal import fftconvolve

    fir_len = 32
    # Box filter (moving-average lowpass)
    ir = np.ones(fir_len, dtype=np.float32) / fir_len

    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {
                "type": "fir",
                "coefficients": ir.tolist()
            }}
        ]
    }

    rng = np.random.default_rng(42)
    audio = rng.standard_normal((2, BLOCK)).astype(np.float32)

    out = run_mimoeq_json(config, audio)

    for ch in range(2):
        ref = fftconvolve(audio[ch].astype(np.float64),
                          ir.astype(np.float64))[:BLOCK].astype(np.float32)
        # Compare central region (exclude filter ramp-in at start and truncation at end)
        np.testing.assert_allclose(
            out[ch, fir_len:BLOCK - fir_len],
            ref[fir_len:BLOCK - fir_len],
            atol=1e-3,
            err_msg=f"FIR ch{ch}: mismatch vs scipy reference"
        )


def test_fir_band_frequency_response():
    """FIR band impulse response matches the specified coefficients."""
    # Single-coefficient FIR (gain=1) = unity/passthrough test for FIR path
    ir = np.array([1.0], dtype=np.float32)

    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {
                "type": "fir",
                "coefficients": ir.tolist()
            }}
        ]
    }

    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0

    out = run_mimoeq_json(config, audio)

    # Impulse through unit FIR should reproduce the impulse at sample 0
    np.testing.assert_allclose(out[0, 0], 1.0, atol=1e-5,
                               err_msg="Unit FIR: impulse amplitude mismatch")
    np.testing.assert_allclose(out[0, 1:50], 0.0, atol=1e-5,
                               err_msg="Unit FIR: post-impulse samples non-zero")


# ===========================================================================
# Tier-2 — MIMO cross-channel routing
# ===========================================================================

def test_mimo_crosschannel_routing():
    """MIMO path: input on ch1 appears on ch2 output (unity gain)."""
    config = {
        "sample_rate": SR,
        "sos": [
            {
                "input_channel": 1,
                "output_channel": 2,
                "parameters": {"type": "gain", "gain_db": 0.0}
            }
        ]
    }
    # Impulse on ch1 only; ch2 is silent
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[0, 0] = 1.0

    out = run_mimoeq_json(config, audio)

    # ch2 (index 1) receives the routed ch1 signal
    np.testing.assert_allclose(out[1, 0], 1.0, atol=1e-5,
                               err_msg="MIMO ch1→ch2: impulse missing on ch2")
    np.testing.assert_allclose(out[1, 1:], 0.0, atol=1e-5,
                               err_msg="MIMO ch1→ch2: spurious tail on ch2")


def test_mimo_path_gain():
    """MIMO path applies gain: output amplitude on target channel is scaled."""
    gain_db = -6.0
    expected = db_to_linear(gain_db)

    config = {
        "sample_rate": SR,
        "sos": [
            {
                "input_channel": 1,
                "output_channel": 2,
                "parameters": {"type": "gain", "gain_db": gain_db}
            }
        ]
    }
    audio = np.full((2, BLOCK), 0.5, dtype=np.float32)
    audio[1, :] = 0.0  # ch2 initially silent

    out = run_mimoeq_json(config, audio)

    # ch2 gets ch1 * expected; ch1 is unchanged (no diagonal, no MIMO targeting it)
    np.testing.assert_allclose(out[1, 512:], 0.5 * expected, atol=1e-4,
                               err_msg="MIMO path gain: ch2 amplitude wrong")
    np.testing.assert_allclose(out[0, 512:], 0.5, atol=1e-5,
                               err_msg="MIMO path gain: ch1 should pass through")


def test_mimo_two_sources_summed_to_one_output():
    """MIMO: two independent source channels summed into one output channel.

    Two sos entries with the *same* (input_channel, output_channel) key are
    collected into a single serial chain (multiplied, not summed).  To test
    the additive workBuffer accumulation we use two *different* source channels
    targeting the same destination: ch1→ch2 and ch2→ch2.  The plugin saves a
    pre-diagonal input snapshot so both paths read the original ch1/ch2 signal
    even though ch2 is also a MIMO output.
    """
    config = {
        "sample_rate": SR,
        "sos": [
            # ch1 → ch2, unity gain
            {"input_channel": 1, "output_channel": 2,
             "parameters": {"type": "gain", "gain_db": 0.0}},
            # ch2 → ch2, unity gain — distinct key, adds into workBuffer[ch2]
            {"input_channel": 2, "output_channel": 2,
             "parameters": {"type": "gain", "gain_db": 0.0}},
        ]
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[0, :] = 0.4   # ch1
    audio[1, :] = 0.3   # ch2

    out = run_mimoeq_json(config, audio)

    # ch2 output = ch1_input + ch2_input = 0.4 + 0.3 = 0.7
    np.testing.assert_allclose(out[1, 512:], 0.7, atol=1e-3,
                               err_msg="Two MIMO sources to same output: sum mismatch")


# ===========================================================================
# Tier-2 — Diagonal channel mask
# ===========================================================================

def test_diagonal_channel_mask_selective():
    """diag_channels=[1]: gain band only applied to ch1, ch2 passes through."""
    gain_db = -12.0
    config = {
        "sample_rate": SR,
        "diag_channels": [1],   # 1-based: only channel 1
        "sos": [
            {"diagonal": True, "parameters": {"type": "gain", "gain_db": gain_db}}
        ]
    }
    audio = np.full((2, BLOCK), 0.5, dtype=np.float32)
    out = run_mimoeq_json(config, audio)

    expected_ch1 = 0.5 * db_to_linear(gain_db)
    expected_ch2 = 0.5  # not in mask — unchanged

    np.testing.assert_allclose(out[0, 512:], expected_ch1, atol=1e-4,
                               err_msg="Diagonal mask: ch1 gain not applied")
    np.testing.assert_allclose(out[1, 512:], expected_ch2, atol=1e-4,
                               err_msg="Diagonal mask: ch2 should be unchanged")


def test_diagonal_channel_mask_empty_means_all():
    """Omitting diag_channels (empty mask) applies diagonal to all channels."""
    gain_db = -6.0
    config = {
        "sample_rate": SR,
        # No diag_channels key → empty mask → applies to all
        "sos": [
            {"diagonal": True, "parameters": {"type": "gain", "gain_db": gain_db}}
        ]
    }
    audio = np.full((2, BLOCK), 0.5, dtype=np.float32)
    out = run_mimoeq_json(config, audio)

    expected = 0.5 * db_to_linear(gain_db)
    np.testing.assert_allclose(out[0, 512:], expected, atol=1e-4,
                               err_msg="Full mask: ch1 gain not applied")
    np.testing.assert_allclose(out[1, 512:], expected, atol=1e-4,
                               err_msg="Full mask: ch2 gain not applied")


# ===========================================================================
# Tier-2 — Combined diagonal + MIMO (serial processing)
# ===========================================================================

def test_diagonal_then_mimo_serial():
    """Diagonal applies first, then MIMO path reads the diagonal-processed signal."""
    gain_diag_db = -6.0    # diagonal gain on ch1
    gain_mimo_db = -6.0    # MIMO path ch1→ch2 gain (reads diagonal output)
    expected = db_to_linear(gain_diag_db) * db_to_linear(gain_mimo_db)

    config = {
        "sample_rate": SR,
        "sos": [
            # Diagonal: attenuate ch1 by -6 dB (all channels, mask omitted)
            {"diagonal": True, "parameters": {"type": "gain", "gain_db": gain_diag_db}},
            # MIMO: route ch1 (after diagonal) to ch2 with additional -6 dB
            {"input_channel": 1, "output_channel": 2,
             "parameters": {"type": "gain", "gain_db": gain_mimo_db}},
        ]
    }
    audio = np.full((2, BLOCK), 0.5, dtype=np.float32)
    out = run_mimoeq_json(config, audio)

    # ch1 output: diagonal-processed (−6 dB)
    np.testing.assert_allclose(out[0, 512:], 0.5 * db_to_linear(gain_diag_db),
                               atol=1e-4, err_msg="Serial: ch1 diagonal gain wrong")
    # ch2 output: diagonal(ch1) → MIMO gain = −12 dB total
    np.testing.assert_allclose(out[1, 512:], 0.5 * expected,
                               atol=1e-4, err_msg="Serial diag+MIMO: ch2 gain wrong")


# ===========================================================================
# Tier-2 — IIR bands via JSON (diagonal, freq-response comparison)
# ===========================================================================

def test_json_peak_freq_response():
    """JSON-loaded diagonal peak band frequency response matches scipy reference."""
    from scipy.signal import fftconvolve

    f, q, db = 1000.0, 1.5, 6.0
    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {
                "type": "peak", "f_Hz": f, "Q": q, "gain_db": db
            }}
        ]
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0   # impulse

    out = run_mimoeq_json(config, audio)
    plugin_ir = out[0]

    sos = make_peak_sos(f, q, db_to_linear(db), SR)
    ref_ir = apply_sos(sos, audio[0])

    np.testing.assert_allclose(
        freq_response_db(plugin_ir), freq_response_db(ref_ir), atol=0.5,
        err_msg="JSON peak: frequency response mismatch"
    )


def test_json_crossover_lp_freq_response():
    """JSON crossover_lp (order=4) frequency response is in the right ballpark."""
    f = 2000.0
    config = {
        "sample_rate": SR,
        "sos": [
            {"diagonal": True, "parameters": {
                "type": "crossover_lp", "f_Hz": f, "order": 4
            }}
        ]
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0

    out = run_mimoeq_json(config, audio)
    ir = out[0]

    # Verify passband (f/4 should be < -1 dB) and stopband (4*f should be < -20 dB)
    freqs = np.fft.rfftfreq(FFT_N, 1.0 / SR)
    spectrum_db = freq_response_db(ir)

    pb_idx = np.argmin(np.abs(freqs - f / 4))
    sb_idx = np.argmin(np.abs(freqs - f * 4))

    assert spectrum_db[pb_idx] > -1.0, \
        f"Crossover LP: passband at {f/4:.0f} Hz below -1 dB ({spectrum_db[pb_idx]:.2f})"
    assert spectrum_db[sb_idx] < -20.0, \
        f"Crossover LP: stopband at {f*4:.0f} Hz above -20 dB ({spectrum_db[sb_idx]:.2f})"


# ===========================================================================
# Tier-2 — Golden regression (complex MIMO config from example file)
# ===========================================================================

GOLDEN_MIMO_TAG = "mimoeq_mimo_example"
EXAMPLE_CONFIG_PATH = os.path.join(
    os.path.dirname(__file__), "fixtures", "mimoeq_example.json"
)


def test_golden_regression_mimo(request):
    """Regression: fixtures/mimoeq_example.json output must match stored golden."""
    if not os.path.exists(EXAMPLE_CONFIG_PATH):
        pytest.skip(f"Example config not found: {EXAMPLE_CONFIG_PATH}")

    with open(EXAMPLE_CONFIG_PATH) as fp:
        config = json.load(fp)

    rng = np.random.default_rng(55)
    audio = rng.standard_normal((2, BLOCK)).astype(np.float32)

    out = run_mimoeq_json(config, audio)

    try:
        update = request.config.getoption("--update-golden")
    except ValueError:
        update = False

    if update:
        save_golden(GOLDEN_MIMO_TAG, out)
        pytest.skip("Golden updated")
    elif not golden_exists(GOLDEN_MIMO_TAG):
        save_golden(GOLDEN_MIMO_TAG, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(GOLDEN_MIMO_TAG)
        np.testing.assert_allclose(out, golden, atol=1e-4,
                                   err_msg="MIMO golden regression failed")


# ===========================================================================
# Tier-2 — Analog-prototype IIR families (Chebyshev I/II, Elliptic, Bessel)
# ===========================================================================
#
# These tests exercise the JSON paths added together with the new
# AnalogPrototypeDesigner. Each test loads the band via a diagonal JSON
# config and inspects the impulse-response spectrum, so the same harness
# proves both the design routine and the JSON round-trip.
#
# We use a wider tolerance (atol=1.0 dB) than the cookbook-biquad tests
# because float32 cascade processing + the per-section coefficient ramp
# introduces a slightly larger error at deep stopband attenuations.


def _spectrum_db_of_diag_band(parameters: dict) -> tuple[np.ndarray, np.ndarray]:
    """Run an impulse through one diagonal band defined by *parameters* and
    return (freqs_Hz, magnitude_dB)."""
    config = {
        "sample_rate": SR,
        "sos": [{"diagonal": True, "parameters": parameters}],
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0   # impulse on both channels
    out = run_mimoeq_json(config, audio)
    freqs = np.fft.rfftfreq(FFT_N, 1.0 / SR)
    return freqs, freq_response_db(out[0])


def _db_at(freqs: np.ndarray, spectrum: np.ndarray, f: float) -> float:
    return spectrum[np.argmin(np.abs(freqs - f))]


# ---- Chebyshev I --------------------------------------------------------

@pytest.mark.parametrize("order", [2, 4, 6])
@pytest.mark.parametrize("ripple_db", [0.1, 1.0, 3.0])
def test_chebyshev1_lp(order, ripple_db):
    """Chebyshev I LP: −3 dB lands near fc, passband ripple ≤ Rp, deep stopband."""
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "chebyshev1_lp", "f_Hz": fc, "order": order,
        "ripple_pass_db": ripple_db,
    })

    cutoff_db = _db_at(freqs, mag, fc)
    assert -4.5 < cutoff_db < -1.5, \
        f"Cheby1 LP order={order} Rp={ripple_db}: −3 dB at fc, got {cutoff_db:.2f}"

    # Passband ripple: well inside the passband (50 Hz .. fc/2) the response
    # stays within the ripple band. We can't measure right up to fc because
    # the transition region is already dropping toward −3 dB by then.
    pb_mask = (freqs > 50) & (freqs < fc * 0.5)
    pb = mag[pb_mask]
    assert pb.max() <  0.5,           f"Cheby1 LP: passband peak above 0 dB ({pb.max():.2f})"
    assert pb.min() > -ripple_db - 0.5, \
        f"Cheby1 LP order={order} Rp={ripple_db}: passband dips below ripple ({pb.min():.2f})"

    # Stopband: at 2×fc the response should be well below cutoff for order ≥ 2.
    stop_db = _db_at(freqs, mag, 2 * fc)
    assert stop_db < -6 * order * 0.5, \
        f"Cheby1 LP order={order}: stopband at 2×fc not steep enough ({stop_db:.2f})"


def test_chebyshev1_lp_high_ripple_does_not_flatten():
    """Regression: Rp > 3 dB used to produce a flat response (acosh(1/ε) NaN).
    Even at 6 dB ripple the filter must still cut by at least 20 dB an octave
    above the cutoff."""
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "chebyshev1_lp", "f_Hz": fc, "order": 4,
        "ripple_pass_db": 6.0,
    })
    stop_db = _db_at(freqs, mag, 2 * fc)
    assert stop_db < -20.0, \
        f"Cheby1 LP Rp=6dB regression: 2×fc should be <-20dB, got {stop_db:.2f}"


@pytest.mark.parametrize("order", [3, 5])
def test_chebyshev1_hp(order):
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "chebyshev1_hp", "f_Hz": fc, "order": order,
        "ripple_pass_db": 1.0,
    })

    cutoff_db = _db_at(freqs, mag, fc)
    assert -4.5 < cutoff_db < -1.5, \
        f"Cheby1 HP order={order}: −3 dB at fc, got {cutoff_db:.2f}"

    # Stopband: well below fc the magnitude must drop significantly.
    stop_db = _db_at(freqs, mag, fc / 4)
    assert stop_db < -6 * order, \
        f"Cheby1 HP order={order}: fc/4 stopband insufficient ({stop_db:.2f})"


# ---- Chebyshev II -------------------------------------------------------

@pytest.mark.parametrize("order, atten", [(4, 60.0), (6, 80.0)])
def test_chebyshev2_lp(order, atten):
    """Chebyshev II LP: flat passband, stopband ≤ −Rs (equiripple)."""
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "chebyshev2_lp", "f_Hz": fc, "order": order,
        "ripple_stop_db": atten,
    })

    cutoff_db = _db_at(freqs, mag, fc)
    assert -4.5 < cutoff_db < -1.5, \
        f"Cheby2 LP order={order}: −3 dB at fc, got {cutoff_db:.2f}"

    # Passband flat to within ~0.5 dB.
    pb = mag[(freqs > 50) & (freqs < fc * 0.5)]
    assert pb.max() < 0.5 and pb.min() > -0.5, \
        f"Cheby2 LP: passband not flat (range {pb.min():.2f}..{pb.max():.2f})"

    # Stopband: Cheby2's equiripple stopband starts at the stopband edge
    # ωs = 1/k (well above fc — the transition is gentler than Cheby1
    # because the zeros sit deep in the stopband). Probe several octaves
    # above fc where we're solidly past the transition.
    stop_db_high = max(_db_at(freqs, mag, 8 * fc),
                        _db_at(freqs, mag, 12 * fc),
                        _db_at(freqs, mag, 16 * fc))
    # freq_response_db clamps at −80 dB so the deepest readable level is ~−80.
    expected = min(-atten + 6.0, -60.0)
    assert stop_db_high < expected, \
        f"Cheby2 LP order={order} A={atten}: deep stopband shallow ({stop_db_high:.2f})"


def test_chebyshev2_hp_passband_flat():
    fc = 800.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "chebyshev2_hp", "f_Hz": fc, "order": 4,
        "ripple_stop_db": 60.0,
    })

    pb = mag[(freqs > fc * 2) & (freqs < 15000)]
    assert pb.max() < 0.5 and pb.min() > -0.5, \
        f"Cheby2 HP: passband not flat (range {pb.min():.2f}..{pb.max():.2f})"

    stop_db = _db_at(freqs, mag, fc / 4)
    assert stop_db < -45.0, \
        f"Cheby2 HP: fc/4 stopband shallow ({stop_db:.2f})"


# ---- Elliptic -----------------------------------------------------------

@pytest.mark.parametrize("order, rp, rs", [(4, 1.0, 60.0), (6, 0.5, 80.0)])
def test_elliptic_lp(order, rp, rs):
    """Elliptic LP: extremely steep transition + equiripple in both bands."""
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "elliptic_lp", "f_Hz": fc, "order": order,
        "ripple_pass_db": rp, "ripple_stop_db": rs,
    })

    cutoff_db = _db_at(freqs, mag, fc)
    assert -4.5 < cutoff_db < -1.5, \
        f"Elliptic LP order={order}: −3 dB at fc, got {cutoff_db:.2f}"

    # Passband ripple bound
    pb = mag[(freqs > 50) & (freqs < fc * 0.8)]
    assert pb.max() <  0.5,        f"Elliptic LP passband peak: {pb.max():.2f}"
    assert pb.min() > -rp - 0.5,   f"Elliptic LP passband dip: {pb.min():.2f}"

    # Even 1.5×fc should already be deep in the stopband for elliptic
    stop_db = _db_at(freqs, mag, 1.5 * fc)
    assert stop_db < -25.0, \
        f"Elliptic LP order={order}: 1.5×fc not steep enough ({stop_db:.2f})"


def test_elliptic_lp_high_ripple_does_not_flatten():
    """Regression: when ripple > 3 dB the −3 dB rescaling bisection used to
    converge to ω = 1 (no rescale), producing a flat response. Now the
    cutoff is correctly placed inside the ripple band, so the magnitude
    must reach the stopband at high frequencies."""
    fc = 1000.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "elliptic_lp", "f_Hz": fc, "order": 4,
        "ripple_pass_db": 4.0, "ripple_stop_db": 60.0,
    })
    # Several octaves up we must be solidly in the elliptic stopband.
    stop_db = max(_db_at(freqs, mag, 4 * fc),
                   _db_at(freqs, mag, 6 * fc),
                   _db_at(freqs, mag, 10 * fc))
    assert stop_db < -50.0, \
        f"Elliptic LP Rp=4 regression: deep stopband shallow, got {stop_db:.2f}"


def test_elliptic_hp_passband_above_cutoff():
    fc = 500.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "elliptic_hp", "f_Hz": fc, "order": 4,
        "ripple_pass_db": 1.0, "ripple_stop_db": 60.0,
    })

    # Above 1.5×fc the response should be in the passband ripple band
    pb = mag[(freqs > fc * 1.5) & (freqs < 15000)]
    assert pb.max() < 0.5 and pb.min() > -1.5, \
        f"Elliptic HP passband not within ripple ({pb.min():.2f}..{pb.max():.2f})"


# ---- Bessel -------------------------------------------------------------

@pytest.mark.parametrize("order", [3, 5, 7])
def test_bessel_lp_group_delay_flat(order):
    """Bessel LP's defining property: nearly constant group delay in the
    passband. Group delay = -dφ/dω; we measure it from the phase of the
    plugin's frequency response on a log grid and assert low variance well
    inside the passband."""
    fc = 2000.0
    config = {
        "sample_rate": SR,
        "sos": [{"diagonal": True, "parameters": {
            "type": "bessel_lp", "f_Hz": fc, "order": order,
        }}],
    }
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[:, 0] = 1.0
    out = run_mimoeq_json(config, audio)

    spectrum = np.fft.rfft(out[0, :FFT_N])
    freqs    = np.fft.rfftfreq(FFT_N, 1.0 / SR)

    # Use only the passband well below fc, where magnitude is still strong
    # enough that the unwrapped phase is meaningful.
    band = (freqs > 50) & (freqs < fc * 0.5)
    phi  = np.unwrap(np.angle(spectrum[band]))
    omega = 2 * np.pi * freqs[band]
    gd_samples = -np.gradient(phi, omega) * SR

    # Group-delay flatness: passband variance should be tiny relative to the mean.
    rel_var = gd_samples.std() / max(abs(gd_samples.mean()), 1e-6)
    assert rel_var < 0.15, \
        f"Bessel LP order={order}: group-delay variance {rel_var:.3f} too high " \
        f"(mean={gd_samples.mean():.1f}, std={gd_samples.std():.2f})"


def test_bessel_hp_passes_high_freq():
    """Sanity: bessel_hp at fc=500 lets 5 kHz through within 1 dB and
    attenuates 50 Hz significantly."""
    fc = 500.0
    freqs, mag = _spectrum_db_of_diag_band({
        "type": "bessel_hp", "f_Hz": fc, "order": 4,
    })
    assert _db_at(freqs, mag, 5000.0) > -1.5
    assert _db_at(freqs, mag,   50.0) < -20.0


# ---- JSON round-trip of ripple parameters --------------------------------

def test_json_ripple_param_roundtrip():
    """Changing ripple_pass_db must change the magnitude response (proves the
    field is actually consumed, not silently dropped)."""
    fc = 1000.0
    _, mag_low  = _spectrum_db_of_diag_band({
        "type": "chebyshev1_lp", "f_Hz": fc, "order": 4, "ripple_pass_db": 0.1,
    })
    _, mag_high = _spectrum_db_of_diag_band({
        "type": "chebyshev1_lp", "f_Hz": fc, "order": 4, "ripple_pass_db": 3.0,
    })
    # The 3 dB-ripple variant must show measurably more passband ripple than 0.1 dB.
    rip_low  = mag_low [(np.fft.rfftfreq(FFT_N, 1.0/SR) > 50) & (np.fft.rfftfreq(FFT_N, 1.0/SR) < fc * 0.8)]
    rip_high = mag_high[(np.fft.rfftfreq(FFT_N, 1.0/SR) > 50) & (np.fft.rfftfreq(FFT_N, 1.0/SR) < fc * 0.8)]
    assert (rip_high.max() - rip_high.min()) > (rip_low.max() - rip_low.min()) + 0.5, \
        "Ripple parameter ignored: 3 dB-ripple variant should ripple more than 0.1 dB one"


# ===========================================================================
# Tier-2 — Linear-phase FIR: Crossover LP + HP sum to flat
# ===========================================================================

def test_crossover_lp_plus_hp_sums_to_flat():
    """Designer-generated crossover_lp and crossover_hp at identical settings
    must satisfy h_LP[n] + h_HP[n] = δ[n - K] (flat magnitude, linear phase).

    Routing trick: feed the same impulse on ch1 and ch2 into a MIMO config
    that sends ch1 through the LP and ch2 through the HP, both summed to
    output ch1. The result on ch1 is LP(x) + HP(x), which should be a pure
    delay → flat magnitude across the spectrum.
    """
    fc, order = 1000.0, 4
    config = {
        "sample_rate": SR,
        "sos": [
            # ch1 → ch1 LP
            {"input_channel": 1, "output_channel": 1,
             "parameters": {"type": "crossover_lp", "f_Hz": fc, "order": order}},
            # ch2 → ch1 HP — different (in,out) key from above so the engine
            # adds rather than cascades them.
            {"input_channel": 2, "output_channel": 1,
             "parameters": {"type": "crossover_hp", "f_Hz": fc, "order": order}},
        ],
    }
    # Identical impulse on both channels.
    audio = np.zeros((2, BLOCK), dtype=np.float32)
    audio[0, 0] = 1.0
    audio[1, 0] = 1.0

    out = run_mimoeq_json(config, audio)
    summed_ir = out[0]   # LP(impulse) + HP(impulse) on ch1

    freqs = np.fft.rfftfreq(FFT_N, 1.0 / SR)
    spec  = freq_response_db(summed_ir)

    # Anywhere in the audible band the magnitude must be ≈ 0 dB.
    band = (freqs > 40) & (freqs < 18000)
    deviation = np.abs(spec[band])
    assert deviation.max() < 0.5, \
        f"Crossover LP+HP not flat: peak deviation {deviation.max():.3f} dB " \
        f"at {freqs[band][deviation.argmax()]:.0f} Hz"


# ===========================================================================
# Tier-2 — Symmetric (linear-phase) FIR group delay
# ===========================================================================

def test_symmetric_fir_group_delay_matches_centre_tap():
    """A symmetric (Type-I) FIR loaded as raw coefficients should produce an
    impulse-response output peaked at sample = (N-1)/2 (+ partitioned-convolver
    overhead for long FIRs). This exercises both:
      • the convolution path (short direct-form vs long MtxConv)
      • the rebuildConvolver fix that adds group-delay to convolverLatency_.

    For lengths above the 128-tap convolver threshold the partitioned engine
    adds one block of latency, so the expected peak shifts by maxBlockSize."""
    for N in (63, 1023):     # short (direct) + long (partitioned)
        # Symmetric FIR: windowed sinc lowpass at fc = SR/8.
        n  = np.arange(N) - (N - 1) // 2
        h  = np.sinc(n / 4.0) * np.hanning(N)
        h /= h.sum()                            # unity DC gain
        # Sanity in the test itself
        assert np.allclose(h, h[::-1], atol=1e-7), "test FIR not symmetric"

        config = {
            "sample_rate": SR,
            "sos": [{"diagonal": True, "parameters": {
                "type": "fir", "coefficients": h.astype(np.float32).tolist(),
            }}],
        }
        audio = np.zeros((2, BLOCK), dtype=np.float32)
        audio[:, 0] = 1.0
        out = run_mimoeq_json(config, audio)
        ir = out[0]

        peak_idx = int(np.argmax(np.abs(ir)))
        group_delay = (N - 1) // 2
        # Long FIRs go through the partitioned convolver which adds up to one
        # buffer of additional latency. Be generous on the upper bound.
        lower = group_delay - 1
        upper = group_delay + 1 if N <= 128 else group_delay + SR        # accept any block size
        assert lower <= peak_idx <= upper, \
            f"Symmetric FIR N={N}: peak at {peak_idx}, expected near {group_delay}"

        # And the peak amplitude is approximately h[(N-1)/2] (= centre tap).
        np.testing.assert_allclose(ir[peak_idx], h[(N - 1) // 2], atol=5e-3,
            err_msg=f"Symmetric FIR N={N}: peak amplitude {ir[peak_idx]:.4f} "
                    f"vs centre-tap {h[(N-1)//2]:.4f}")
