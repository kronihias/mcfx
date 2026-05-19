#!/usr/bin/env python3
"""
mcfx_convolver performance benchmark.

Measures CPU throughput for a large FIR convolution matrix, capturing load
across all background MtxConvSlave threads via resource.getrusage(RUSAGE_CHILDREN).

Typical workflow (comparing two commits):

    git checkout <before-commit>
    cmake --build _build --target mcfx_testhost mcfx_convolver_VST3 -j8
    python scripts/bench/bench_convolver.py --label "before"

    git checkout <after-commit>
    cmake --build _build --target mcfx_testhost mcfx_convolver_VST3 -j8
    python scripts/bench/bench_convolver.py --label "after"

Metrics reported:
  wall_s        Wall-clock seconds for the full processing run
  cpu_s         Total CPU seconds across all threads (user + sys)
  rt_factor     cpu_s / audio_duration — CPU budget fraction (cores consumed)
  throughput_x  audio_duration / wall_s — how many times faster than real-time
  cpu_load_pct  cpu_s / wall_s * 100 — average parallelism utilised
"""

import argparse
import json
import os
import resource
import subprocess
import sys
import tempfile
import time

import numpy as np

try:
    import soundfile as sf
except ImportError:
    sys.exit("soundfile not installed — pip install soundfile")

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_TESTHOST = os.path.join(REPO_ROOT, "_build", "testhost", "mcfx_testhost")
DEFAULT_PLUGIN   = os.path.join(REPO_ROOT, "_build", "vst3", "mcfx_convolver.vst3")

# ---------------------------------------------------------------------------
# Fixture generation
# ---------------------------------------------------------------------------

def generate_ir_wav(path: str, n_in: int, n_out: int, taps: int, fs: int) -> None:
    """
    Write a packedmatrix IR WAV: n_out channels, each containing n_in
    concatenated IRs of `taps` samples.  Each IR is a unit dirac at sample 0,
    giving a dense identity-like routing matrix.  The FFT convolution work is
    identical to any other IR — only the output values differ.
    """
    total_samples = n_in * taps
    # Shape: (total_samples, n_out) — soundfile writes (frames, channels)
    data = np.zeros((total_samples, n_out), dtype=np.float32)
    for in_ch in range(n_in):
        data[in_ch * taps, :] = 1.0  # dirac at start of each block, all outputs
    sf.write(path, data, fs, subtype="FLOAT")


def generate_config(path: str, ir_wav_basename: str, n_in: int) -> None:
    """Write a convolver config using /impulse/packedmatrix."""
    with open(path, "w") as f:
        f.write(f"/impulse/packedmatrix {n_in} 1.0 0 0 0 {ir_wav_basename}\n")


def generate_input_audio(path: str, n_ch: int, n_samples: int, fs: int) -> None:
    """Write low-amplitude white noise to a WAV file."""
    rng = np.random.default_rng(42)
    audio = (rng.standard_normal((n_samples, n_ch)) * 0.1).astype(np.float32)
    sf.write(path, audio, fs, subtype="FLOAT")


# ---------------------------------------------------------------------------
# Single benchmark run
# ---------------------------------------------------------------------------

def run_once(testhost: str, plugin: str, fixture_dir: str,
             n_ch: int, block_size: int, fs: int) -> dict:
    """
    Run testhost on pre-generated fixtures, return timing dict.
    resource.getrusage(RUSAGE_CHILDREN) accumulates CPU time across ALL threads
    of the child process — exactly what we need for background-threaded convolution.
    """
    config_path = os.path.join(fixture_dir, "config.txt")
    in_wav      = os.path.join(fixture_dir, "input.wav")
    params_json = os.path.join(fixture_dir, "params.json")

    params = {"Config File": config_path}
    with open(params_json, "w") as f:
        json.dump(params, f)

    cmd = [
        testhost,
        "--plugin",    plugin,
        "--params",    params_json,
        "--input",     in_wav,
        "--channels",  str(n_ch),
        "--blocksize", str(block_size),
        "--samplerate", str(fs),
        "--no-output",          # skip WAV write — pure CPU benchmark
    ]

    ru_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    t0 = time.perf_counter()

    result = subprocess.run(cmd, capture_output=True)

    wall = time.perf_counter() - t0
    ru_after = resource.getrusage(resource.RUSAGE_CHILDREN)

    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode(errors="replace"))
        sys.exit(f"testhost exited with code {result.returncode}")

    user_cpu = ru_after.ru_utime - ru_before.ru_utime
    sys_cpu  = ru_after.ru_stime - ru_before.ru_stime
    cpu      = user_cpu + sys_cpu

    return {"wall_s": wall, "cpu_s": cpu, "user_s": user_cpu, "sys_s": sys_cpu}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--testhost", default=DEFAULT_TESTHOST, help="Path to mcfx_testhost binary")
    p.add_argument("--plugin",   default=DEFAULT_PLUGIN,   help="Path to mcfx_convolver.vst3")
    p.add_argument("--n-in",     type=int, default=84,     help="Number of input channels")
    p.add_argument("--n-out",    type=int, default=36,     help="Number of output channels")
    p.add_argument("--taps",     type=int, default=4096,   help="FIR filter length (samples)")
    p.add_argument("--duration", type=float, default=30.0, help="Audio duration in seconds")
    p.add_argument("--blocksize",type=int, default=512,    help="Processing block size")
    p.add_argument("--samplerate",type=int,default=48000,  help="Sample rate (Hz)")
    p.add_argument("--runs",     type=int, default=3,      help="Number of benchmark runs (median reported)")
    p.add_argument("--label",    default="",               help="Label for this result (e.g. 'before', 'after')")
    p.add_argument("--keep-fixtures", action="store_true", help="Keep generated fixture files after run")
    args = p.parse_args()

    # Validate binaries
    for name, path in [("testhost", args.testhost), ("plugin", args.plugin)]:
        if not os.path.exists(path):
            sys.exit(
                f"ERROR: {name} not found at {path}\n"
                f"Build first:  cmake --build _build --target mcfx_testhost mcfx_convolver_VST3"
            )

    n_ch      = max(args.n_in, args.n_out)  # testhost uses one channel count for both buses
    n_samples = int(args.duration * args.samplerate)
    audio_dur = n_samples / args.samplerate

    label_str = f" [{args.label}]" if args.label else ""
    print(f"\nmcfx_convolver benchmark{label_str}")
    print(f"  Matrix:     {args.n_in} in × {args.n_out} out  ({args.n_in * args.n_out} filters)")
    print(f"  FIR taps:   {args.taps}  ({args.taps / args.samplerate * 1000:.1f} ms)")
    print(f"  Audio:      {audio_dur:.0f} s  @ {args.samplerate} Hz  block={args.blocksize}")
    print(f"  Runs:       {args.runs}")
    print()

    # Generate fixtures (reuse a temp dir or a named dir)
    fixture_ctx = tempfile.TemporaryDirectory() if not args.keep_fixtures else None
    if fixture_ctx:
        fixture_dir = fixture_ctx.name
    else:
        fixture_dir = os.path.join(REPO_ROOT, "tests", "_bench_fixtures")
        os.makedirs(fixture_dir, exist_ok=True)

    try:
        ir_wav      = os.path.join(fixture_dir, "ir_matrix.wav")
        config_path = os.path.join(fixture_dir, "config.txt")
        in_wav      = os.path.join(fixture_dir, "input.wav")

        print(f"Generating IR WAV ({args.n_out} ch × {args.n_in * args.taps} samples)…")
        generate_ir_wav(ir_wav, args.n_in, args.n_out, args.taps, args.samplerate)

        print(f"Generating config…")
        generate_config(config_path, "ir_matrix.wav", args.n_in)

        print(f"Generating input audio ({n_ch} ch × {n_samples} samples)…")
        generate_input_audio(in_wav, n_ch, n_samples, args.samplerate)
        print()

        # Run benchmark
        results = []
        for run in range(args.runs):
            print(f"  Run {run + 1}/{args.runs}…", end="", flush=True)
            r = run_once(
                args.testhost, args.plugin, fixture_dir,
                n_ch, args.blocksize, args.samplerate
            )
            r["throughput_x"]  = audio_dur / r["wall_s"]
            r["rt_factor"]     = r["cpu_s"] / audio_dur
            r["cpu_load_pct"]  = r["cpu_s"] / r["wall_s"] * 100.0
            results.append(r)
            print(f"  wall={r['wall_s']:.2f}s  cpu={r['cpu_s']:.2f}s  "
                  f"throughput={r['throughput_x']:.1f}×  rt_factor={r['rt_factor']:.2f}")

        # Compute medians
        def med(key):
            return float(np.median([r[key] for r in results]))

        print()
        print("=" * 60)
        if args.label:
            print(f"  Result: {args.label}")
        print(f"  wall_s        {med('wall_s'):.3f} s")
        print(f"  cpu_s         {med('cpu_s'):.3f} s   (user={med('user_s'):.2f}  sys={med('sys_s'):.2f})")
        print(f"  throughput_x  {med('throughput_x'):.2f}×   (wall-clock real-time multiple)")
        print(f"  rt_factor     {med('rt_factor'):.3f}   (cpu / audio_dur; lower = less CPU per core)")
        print(f"  cpu_load_pct  {med('cpu_load_pct'):.1f} %  (avg thread parallelism used)")
        print("=" * 60)
        print()
        print("Notes:")
        print("  throughput_x > 1  → faster than real-time (wall-clock)")
        print("  rt_factor < N_cores → overall CPU budget within limits")
        print()
        print("To compare commits:")
        print("  Build before-commit, run with --label before")
        print("  Build after-commit,  run with --label after")
        print("  Compare throughput_x (higher is better) and rt_factor (lower is better)")

    finally:
        if fixture_ctx:
            fixture_ctx.cleanup()


if __name__ == "__main__":
    main()
