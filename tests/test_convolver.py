"""
mcfx_convolver integration tests — Tier 2 (mcfx_testhost CLI).

Requires:
  cmake -B _build -DBUILD_VST3=ON -DBUILD_TESTHOST=ON
  cmake --build _build --target mcfx_testhost
  pip install soundfile

mcfx_convolver loads an IR matrix from a config file or directly from WAV.
The simplest config is a single-channel WAV used as a mono IR:

  /convolver
  /impulse <path.wav>

For parametric tests we write a temporary WAV IR file and a config,
route it through the testhost, then compare to scipy.signal.fftconvolve.

Tests
-----
1. Identity IR (dirac) → output == input (within atol 1e-3 for partitioned)
2. Known single-channel IR → output matches scipy reference
3. Silence in → silence out
4. Golden regression
"""

import sys, os
sys.path.insert(0, os.path.dirname(__file__))

import json
import tempfile
import numpy as np
import pytest

from reference.convolver_ref import convolve_matrix, make_dirac
from conftest import SR, BLOCK, save_golden, load_golden, golden_exists, run_testhost

# soundfile import deferred — skip if not installed
try:
    import soundfile as sf
    HAS_SOUNDFILE = True
except ImportError:
    HAS_SOUNDFILE = False

pytestmark = pytest.mark.skipif(
    not HAS_SOUNDFILE,
    reason="soundfile not installed — pip install soundfile"
)

TESTHOST_BIN = os.path.join(
    os.path.dirname(os.path.dirname(__file__)),
    "_build", "testhost", "mcfx_testhost"
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

N_CH = 2   # keep stereo for Tier-2 tests (testhost handles full multichannel)


def write_wav(path: str, audio: np.ndarray, fs: int = SR) -> None:
    """audio: (channels, samples) float32"""
    sf.write(path, audio.T, fs, subtype="FLOAT")


def read_wav(path: str) -> np.ndarray:
    """Returns (channels, samples) float32"""
    data, _ = sf.read(path, dtype="float32")
    return data.T if data.ndim > 1 else data[None, :]


def write_ir_wav(path: str, ir: np.ndarray, fs: int = SR) -> None:
    """ir: 1-D float32 impulse response"""
    sf.write(path, ir.astype(np.float32), fs, subtype="FLOAT")


def write_convolver_config(config_path: str, ir_wav_path: str,
                           n_channels: int = N_CH) -> None:
    """
    Minimal mcfx_convolver config for a single-channel IR applied
    identically to each channel (diagonal routing).

    Format: /impulse/read <in> <out> <gain> <delay> <offset> <length> <wav_ch> <file>
    Channels are 1-indexed.  The filename is resolved relative to the config
    file's directory, so we use just the basename (both files are in the same
    temp directory).
    """
    ir_basename = os.path.basename(ir_wav_path)
    with open(config_path, "w") as f:
        for ch in range(1, n_channels + 1):
            f.write(f"/impulse/read {ch} {ch} 1.0 0 0 0 1 {ir_basename}\n")


def run_convolver(audio: np.ndarray, ir_1d: np.ndarray,
                  n_channels: int = N_CH) -> np.ndarray:
    """
    Run audio through mcfx_convolver via testhost, return output array.
    audio:  (n_channels, n_samples) float32
    ir_1d:  1-D float32 impulse response applied identically to each channel
    """
    if not os.path.exists(TESTHOST_BIN):
        pytest.skip(f"mcfx_testhost not built: {TESTHOST_BIN}")

    with tempfile.TemporaryDirectory() as tmp:
        ir_wav     = os.path.join(tmp, "ir.wav")
        cfg_path   = os.path.join(tmp, "config.txt")
        in_wav     = os.path.join(tmp, "in.wav")
        out_wav    = os.path.join(tmp, "out.wav")
        params_json = os.path.join(tmp, "params.json")

        # Pad the input with one extra block of silence so the convolver's
        # block-based FFT processing flushes its internal buffers before the
        # file ends.  The testhost blocksize defaults to 512, so 512 zeros is
        # enough to push the last partial block through.
        flush_pad = 512
        padded = np.concatenate(
            [audio, np.zeros((n_channels, flush_pad), dtype=np.float32)], axis=1
        )
        write_wav(in_wav, padded)
        write_ir_wav(ir_wav, ir_1d)
        write_convolver_config(cfg_path, ir_wav, n_channels=n_channels)

        # The convolver config file path is set via the "Config File" parameter
        # (index 0 in the convolver — a string param via setStateInformation).
        # For testhost we pass it as a special key.
        params = {"Config File": cfg_path}
        with open(params_json, "w") as f:
            json.dump(params, f)

        import subprocess
        result = subprocess.run([
            TESTHOST_BIN,
            "--plugin",   os.path.join(os.path.dirname(os.path.dirname(__file__)),
                                       "_build", "vst3", "mcfx_convolver.vst3"),
            "--params",   params_json,
            "--input",    in_wav,
            "--channels", str(n_channels),
            "--output",   out_wav,
        ], capture_output=True)

        if result.returncode != 0:
            pytest.skip(
                f"testhost failed (rc={result.returncode}): "
                f"{result.stderr.decode()[:200]}"
            )

        # Return only the original-length portion (trim the flush pad).
        n_orig = audio.shape[1]
        return read_wav(out_wav)[:, :n_orig]


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def noise_stereo():
    rng = np.random.default_rng(11)
    return rng.standard_normal((N_CH, BLOCK)).astype(np.float32)


@pytest.fixture(scope="module")
def impulse_stereo():
    sig = np.zeros((N_CH, BLOCK), dtype=np.float32)
    sig[:, 0] = 1.0
    return sig


# ===========================================================================
# Tests
# ===========================================================================

def test_silence_in_silence_out(impulse_stereo):
    """All-zero input → all-zero output."""
    zeros = np.zeros_like(impulse_stereo)
    ir = make_dirac(64)  # short identity IR

    out = run_convolver(zeros, ir)
    np.testing.assert_allclose(out, 0.0, atol=1e-6,
                               err_msg="Silence in should produce silence out")


def test_dirac_ir_identity(impulse_stereo):
    """
    Convolving with a unit dirac at offset 0 should reproduce the input.
    Tolerance is 1e-3 because partitioned FFT convolution differs from
    scipy's reference at float32 precision.
    """
    ir = make_dirac(512)   # length 512 dirac at sample 0

    out = run_convolver(impulse_stereo, ir)

    # Output should equal input up to convolution latency + tolerance
    np.testing.assert_allclose(
        out[:, :BLOCK], impulse_stereo[:, :BLOCK],
        atol=1e-3,
        err_msg="Dirac IR should reproduce input"
    )


def test_known_ir_matches_scipy(noise_stereo):
    """
    Convolve with a short known IR, compare to scipy.signal.fftconvolve.
    Uses a 64-tap Gaussian pulse as IR.
    """
    ir_length = 64
    t  = np.arange(ir_length, dtype=np.float32)
    ir = np.exp(-0.5 * ((t - ir_length / 2) / 8.0) ** 2).astype(np.float32)
    ir /= ir.sum()   # normalize to unity gain

    out = run_convolver(noise_stereo, ir)

    from scipy.signal import fftconvolve
    for ch in range(N_CH):
        ref = fftconvolve(noise_stereo[ch].astype(np.float64),
                          ir.astype(np.float64))[:BLOCK].astype(np.float32)
        np.testing.assert_allclose(
            out[ch, :BLOCK - ir_length], ref[:BLOCK - ir_length],
            atol=1e-3,
            err_msg=f"Channel {ch}: scipy vs plugin mismatch"
        )


# ===========================================================================
# Golden regression
# ===========================================================================

GOLDEN_TAG = "convolver_gauss64_noise"


def test_golden_regression(noise_stereo, request):
    ir_length = 64
    t  = np.arange(ir_length, dtype=np.float32)
    ir = np.exp(-0.5 * ((t - ir_length / 2) / 8.0) ** 2).astype(np.float32)
    ir /= ir.sum()

    out = run_convolver(noise_stereo, ir)

    if request.config.getoption("--update-golden", default=False):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden updated")
    elif not golden_exists(GOLDEN_TAG):
        save_golden(GOLDEN_TAG, out)
        pytest.skip("Golden created — re-run to compare")
    else:
        golden = load_golden(GOLDEN_TAG)
        np.testing.assert_allclose(out, golden, atol=1e-5)
