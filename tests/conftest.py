"""
pytest fixtures for mcfx plugin tests.

Plugins are loaded once per test session (scope="session") to avoid
repeated disk I/O and plugin initialisation overhead.

pedalboard is used for Tier-1 (per-channel, ≤8 ch) tests.
mcfx_testhost CLI binary is used for Tier-2 multichannel tests.
"""

import os
import shutil
import sys
import numpy as np
import pytest

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VST3_DIR  = os.path.join(REPO_ROOT, "_build", "vst3")

# mcfx_testhost binary (built with BUILD_TESTHOST=ON)
_EXE_SUFFIX = ".exe" if sys.platform == "win32" else ""
TESTHOST_BIN = os.path.join(REPO_ROOT, "_build", "testhost",
                            f"mcfx_testhost{_EXE_SUFFIX}")

GOLDEN_DIR = os.path.join(os.path.dirname(__file__), "golden")


def vst3(name: str) -> str:
    return os.path.join(VST3_DIR, f"{name}.vst3")


def vst3_load_path(bundle_path: str) -> str:
    # Pedalboard on Windows fails to recognise a JUCE-built VST3 bundle dir
    # (no desktop.ini marker), but loads the inner DLL fine. Resolve to it.
    if sys.platform == "win32" and os.path.isdir(bundle_path):
        inner = os.path.join(bundle_path, "Contents", "x86_64-win",
                             os.path.basename(bundle_path))
        if os.path.isfile(inner):
            return inner
    return bundle_path


# ---------------------------------------------------------------------------
# pedalboard plugin fixtures (Tier-1)
# ---------------------------------------------------------------------------

class PluginWrapper:
    """Wraps a pedalboard plugin to support display-name item access.

    pedalboard >= 0.9 removed __getitem__/__setitem__ from VST3Plugin.
    Tests use plugin["Param Name"] = value; this wrapper restores that
    by mapping display names to Python attribute names at construction time.
    """

    def __init__(self, plugin):
        self._plugin = plugin
        self._name_map = {p.name: k for k, p in plugin.parameters.items()}

    def __setitem__(self, display_name: str, value):
        py_name = self._name_map.get(display_name)
        if py_name is None:
            raise KeyError(
                f"No parameter {display_name!r}. "
                f"Available: {sorted(self._name_map)}"
            )
        # Set via raw_value (normalised 0–1 float) to match the old pedalboard
        # API that the tests were written for.  The new typed setattr API
        # requires actual units (Hz, dB, bool, str) which would need every
        # test rewritten.
        self._plugin.parameters[py_name].raw_value = float(value)

    def __getitem__(self, display_name: str):
        py_name = self._name_map.get(display_name)
        if py_name is None:
            raise KeyError(display_name)
        return self._plugin.parameters[py_name].raw_value

    def __call__(self, *args, **kwargs):
        return self._plugin(*args, **kwargs)

    def __getattr__(self, name):
        return getattr(self._plugin, name)


def _load(name: str):
    try:
        import pedalboard
    except ImportError:
        pytest.skip("pedalboard not installed — run: pip install pedalboard")
    path = vst3(name)
    if not os.path.exists(path):
        pytest.skip(f"Plugin not built: {path}")
    return PluginWrapper(pedalboard.load_plugin(vst3_load_path(path)))


@pytest.fixture(scope="session")
def plugin_filter():
    return _load("mcfx_filter")


@pytest.fixture(scope="session")
def plugin_gain_delay():
    return _load("mcfx_gain_delay")


@pytest.fixture(scope="session")
def plugin_delay():
    return _load("mcfx_delay")


# ---------------------------------------------------------------------------
# Common test signals
# ---------------------------------------------------------------------------

SR = 48000
BLOCK = SR  # 1 second of audio at 48 kHz


@pytest.fixture(scope="session")
def white_noise_stereo() -> np.ndarray:
    """Stereo (2, SR) float32 white noise, reproducible."""
    rng = np.random.default_rng(42)
    return rng.standard_normal((2, BLOCK)).astype(np.float32)


@pytest.fixture(scope="session")
def impulse_mono() -> np.ndarray:
    """Mono impulse at sample 0."""
    sig = np.zeros(BLOCK, dtype=np.float32)
    sig[0] = 1.0
    return sig


@pytest.fixture(scope="session")
def impulse_stereo(impulse_mono) -> np.ndarray:
    return np.stack([impulse_mono, impulse_mono])


# ---------------------------------------------------------------------------
# Golden file helpers
# ---------------------------------------------------------------------------

def load_golden(name: str) -> np.ndarray:
    return np.load(os.path.join(GOLDEN_DIR, f"{name}.npy"))


def save_golden(name: str, array: np.ndarray) -> None:
    np.save(os.path.join(GOLDEN_DIR, f"{name}.npy"), array)


def golden_exists(name: str) -> bool:
    return os.path.exists(os.path.join(GOLDEN_DIR, f"{name}.npy"))


# ---------------------------------------------------------------------------
# CLI flag for refreshing golden files (must live in conftest so it registers
# before pytest argument parsing — test modules load too late).
# ---------------------------------------------------------------------------

def pytest_addoption(parser):
    parser.addoption("--update-golden", action="store_true", default=False,
                     help="Overwrite golden reference files")
    parser.addoption("--bench", action="store_true", default=False,
                     help="Run benchmarks (off by default — they take minutes)")


# ---------------------------------------------------------------------------
# Tier-2 CLI host helper
# ---------------------------------------------------------------------------

def run_testhost(plugin_name: str, params: dict, audio: np.ndarray,
                 fs: int = SR, compensate_latency: bool = True) -> np.ndarray:
    """
    Call the mcfx_testhost CLI binary, return processed audio as numpy array.

    Requires:
      - BUILD_TESTHOST=ON when configuring CMake
      - soundfile Python package: pip install soundfile
    """
    import json
    import subprocess
    import tempfile
    try:
        import soundfile as sf
    except ImportError:
        pytest.skip("soundfile not installed — run: pip install soundfile")

    if not os.path.exists(TESTHOST_BIN):
        pytest.skip(f"mcfx_testhost not built: {TESTHOST_BIN}")

    plugin_path = vst3(plugin_name)
    if not os.path.exists(plugin_path):
        pytest.skip(f"Plugin not built: {plugin_path}")

    with tempfile.TemporaryDirectory() as tmp:
        in_wav   = os.path.join(tmp, "in.wav")
        out_wav  = os.path.join(tmp, "out.wav")
        par_json = os.path.join(tmp, "params.json")

        sf.write(in_wav, audio.T, fs, subtype="FLOAT")
        with open(par_json, "w") as f:
            json.dump(params, f)

        cmd = [
            TESTHOST_BIN,
            "--plugin",   plugin_path,
            "--params",   par_json,
            "--input",    in_wav,
            "--channels", str(audio.shape[0]),
            "--output",   out_wav,
        ]
        if not compensate_latency:
            cmd.append("--no-latency-compensation")
        subprocess.run(cmd, check=True, capture_output=True)

        data, _ = sf.read(out_wav, dtype="float32")
        return data.T  # (channels, samples)
