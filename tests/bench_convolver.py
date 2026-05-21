"""
mcfx_convolver performance benchmark.

Drives the testhost with representative configurations and reports the audio
processing wall-time (excluding plugin load, IR loading, and WAV I/O) and the
resulting realtime factor. Intended for bracketing FFT-implementation swaps
(FFTW vs chowdsp_fft / pffft / ...) — capture the table before and after,
diff the numbers.

Skipped by default. Run with:

    pytest tests/bench_convolver.py --bench -s

The `-s` is required so the table is printed to the terminal.

Notes
-----
* Each configuration runs a 30 s noise signal through a diagonal IR matrix
  (one IR per channel pair).  30 s is long enough to amortise startup costs
  even on small configurations, and the testhost only reports the time spent
  inside the processBlock loop, so plugin-load / IR-load overhead doesn't
  contaminate the measurement.
* IR length spans the partitioned-convolver's regimes: short (1024) is one
  full minimum-partition; medium (8192) hits the MaxPartSize boundary; long
  (32768, 131072) exercises the multi-partition gardener scheme.
* Channel counts go up to 32 to keep the wall time bounded; bump
  CHANNEL_COUNTS if you want to stress further.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass

import numpy as np
import pytest

from conftest import SR, TESTHOST_BIN, vst3

try:
    import soundfile as sf
    HAS_SF = True
except ImportError:
    HAS_SF = False


# Module-level skip: opt-in via --bench, plus the usual prereqs.
def _check(request) -> None:
    if not request.config.getoption("--bench", default=False):
        pytest.skip("benchmarks disabled — pass --bench to run")
    if not HAS_SF:
        pytest.skip("soundfile not installed")
    if not os.path.exists(TESTHOST_BIN):
        pytest.skip(f"mcfx_testhost not built: {TESTHOST_BIN}")
    if not os.path.exists(vst3("mcfx_convolver")):
        pytest.skip("mcfx_convolver not built")


# ---------------------------------------------------------------------------
# Configurations under test
# ---------------------------------------------------------------------------

AUDIO_SECONDS = 30
N_SAMPLES = AUDIO_SECONDS * SR

CONFIGS: list[tuple[int, int]] = [
    # (channels, IR length in samples)
    ( 2,    1024),
    ( 2,    8192),
    ( 8,    8192),
    ( 8,   32768),
    ( 8,  131072),  # ~2.7s @48kHz — exercises larger partitions
    (16,    8192),
    (32,    8192),
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

@dataclass
class BenchResult:
    n_channels: int
    ir_length: int
    process_seconds: float
    realtime_factor: float


def _gen_ir(length: int, seed: int = 42) -> np.ndarray:
    """Pseudo-random IR with exponential decay — realistic energy distribution."""
    rng = np.random.default_rng(seed)
    n = np.arange(length, dtype=np.float64)
    env = np.exp(-3.0 * n / length)
    h = rng.standard_normal(length) * env
    h /= np.linalg.norm(h) + 1e-12
    return h.astype(np.float32)


def _gen_input(n_channels: int, seed: int = 1) -> np.ndarray:
    """Gaussian noise, channel-decorrelated, scaled to avoid clipping."""
    rng = np.random.default_rng(seed)
    return (rng.standard_normal((n_channels, N_SAMPLES))
            .astype(np.float32) * 0.1)


def _write_config_diagonal(cfg_path: str, ir_basename: str, n_channels: int) -> None:
    with open(cfg_path, "w") as f:
        for ch in range(1, n_channels + 1):
            f.write(f"/impulse/read {ch} {ch} 1.0 0 0 0 1 {ir_basename}\n")


_TIMING_RE = re.compile(
    r"process_seconds=([\d.eE+-]+).*?realtime_factor=([\d.eE+-]+)"
)


def _run_one(n_channels: int, ir_length: int) -> BenchResult:
    with tempfile.TemporaryDirectory() as tmp:
        ir = _gen_ir(ir_length)
        ir_wav = os.path.join(tmp, "ir.wav")
        sf.write(ir_wav, ir, SR, subtype="FLOAT")

        cfg = os.path.join(tmp, "config.txt")
        _write_config_diagonal(cfg, "ir.wav", n_channels)

        audio = _gen_input(n_channels)
        in_wav = os.path.join(tmp, "in.wav")
        sf.write(in_wav, audio.T, SR, subtype="FLOAT")

        params_json = os.path.join(tmp, "params.json")
        with open(params_json, "w") as f:
            json.dump({"Config File": cfg}, f)

        cmd = [
            TESTHOST_BIN,
            "--plugin",   vst3("mcfx_convolver"),
            "--params",   params_json,
            "--input",    in_wav,
            "--channels", str(n_channels),
            "--no-output",
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        m = _TIMING_RE.search(result.stderr)
        if not m:
            pytest.fail(
                "Could not parse timing from testhost stderr.\n"
                f"stderr:\n{result.stderr}\nstdout:\n{result.stdout}"
            )
        return BenchResult(
            n_channels=n_channels,
            ir_length=ir_length,
            process_seconds=float(m.group(1)),
            realtime_factor=float(m.group(2)),
        )


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

# Module-scoped accumulator so the per-config tests can populate it and a
# finaliser can print the table.
_results: list[BenchResult] = []


@pytest.mark.parametrize("n_channels,ir_length", CONFIGS)
def test_realtime_factor(request, n_channels: int, ir_length: int) -> None:
    _check(request)
    r = _run_one(n_channels, ir_length)
    _results.append(r)
    # One-line summary per config so progress is visible during a long run.
    sys.stdout.write(
        f"\n  {n_channels:3d}ch x {ir_length:6d}-tap: "
        f"{r.process_seconds:7.2f}s process, {r.realtime_factor:8.2f}x realtime\n"
    )
    sys.stdout.flush()


@pytest.fixture(scope="module", autouse=True)
def _print_summary(request):
    """Print a clean table after the parametrized cases finish."""
    yield
    if not _results:
        return
    print()
    print(f"mcfx_convolver benchmark - SR={SR}Hz, audio={AUDIO_SECONDS}s, "
          f"testhost={os.path.basename(TESTHOST_BIN)}")
    print("=" * 64)
    print(f"  {'config':<27}  {'process':>10}  {'realtime':>10}")
    print("-" * 64)
    for r in _results:
        cfg = f"{r.n_channels:3d}ch x {r.ir_length:6d}-tap diagonal"
        print(f"  {cfg:<27}  {r.process_seconds:>8.2f}s  {r.realtime_factor:>8.2f}x")
    print()
    # Markdown table — paste directly into tests/BENCHMARKS.md.
    print("Markdown for tests/BENCHMARKS.md:")
    print()
    print("| config                       | process (s) | realtime |")
    print("|------------------------------|------------:|---------:|")
    for r in _results:
        cfg = f"{r.n_channels}ch x {r.ir_length}-tap diagonal"
        print(f"| {cfg:<28} | {r.process_seconds:>11.2f} | "
              f"{r.realtime_factor:>7.0f}x |")
    print()
