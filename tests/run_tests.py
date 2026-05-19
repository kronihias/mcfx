#!/usr/bin/env python3
"""
Configure, build, and run the mcfx test suite. Same invocation on
macOS, Linux, and Windows:

    python tests/run_tests.py

Stages (in order):
  1. cmake configure  _build/  with the test-only flag set
  2. cmake build      everything (plugins + mcfx_testhost + net loopback)
  3. pip install      tests/requirements.txt   (skip with --no-pip)
  4. pytest           tests/                   (skip with --no-tests)

Any of the build stages can be skipped with --no-configure / --no-build
when iterating. Extra pytest arguments can be forwarded after `--`:

    python tests/run_tests.py -- -k filter -x

CI uses this exact entrypoint; keep it green on both platforms.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "_build"
WIN_LIBS  = REPO_ROOT / "win-libs"
IS_WINDOWS = sys.platform == "win32"
IS_MACOS   = sys.platform == "darwin"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print(f"\n$ {' '.join(str(c) for c in cmd)}", flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def cmake_configure() -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    args = [
        "cmake", "-S", str(REPO_ROOT), "-B", str(BUILD_DIR),
        "-DBUILD_VST3=ON",
        # Skip the VST2 build: avoids the private Steinberg SDK dependency in
        # CI. mcfx_anything / mcfx_graph still build, just without VST2 plugin
        # hosting, which the tests don't exercise.
        "-DBUILD_VST=OFF",
        "-DBUILD_TESTHOST=ON",
        "-DBUILD_NET_TESTS=ON",
    ]

    if IS_WINDOWS:
        # FFTW3 ships in-tree under win-libs/ for mcfx_convolver. Mirrors
        # scripts/build_all_win64.bat.
        args += [
            f"-DFFTW3_INCLUDE_DIR={WIN_LIBS.as_posix()}/",
            f"-DFFTW3F_LIBRARY={WIN_LIBS.as_posix()}/x64/libfftw3f-3.lib",
        ]

    # Prefer Ninja on macOS/Linux when available — much faster than make.
    if not IS_WINDOWS and shutil.which("ninja"):
        args += ["-G", "Ninja"]

    run(args)


# Targets the pytest suite needs. We build these by name (rather than the
# default "all") so an unrelated plugin failing to build doesn't block the
# test run — known case: mcfx_anything and mcfx_graph host plugins and call
# AudioProcessor::createEditor() directly, which was made private in JUCE
# 8.0.13. Both their tests skip gracefully when the bundle is absent
# (tests/test_anything.py, tests/test_smoke_loads.py).
BUILD_TARGETS = [
    "mcfx_testhost",
    "mcfx_net_loopback_test",
    "mcfx_convolver_VST3",
    "mcfx_delay_VST3",
    "mcfx_filter_VST3",
    "mcfx_gain_delay_VST3",
    "mcfx_meter_VST3",
    "mcfx_mimoeq_VST3",
    "mcfx_send_VST3",
    "mcfx_receive_VST3",
]


def cmake_build() -> None:
    args = ["cmake", "--build", str(BUILD_DIR), "--config", "Release",
            "--parallel", "--target", *BUILD_TARGETS]
    run(args)

    if IS_WINDOWS:
        # On Windows the FFTW3 DLL has to sit alongside any binary that links
        # against it (mcfx_convolver.vst3 / mcfx_testhost) — Windows resolves
        # imports from the executable directory, not the lib search path.
        dll = WIN_LIBS / "x64" / "libfftw3f-3.dll"
        if dll.is_file():
            for dst in [
                BUILD_DIR / "testhost",
                BUILD_DIR / "vst3" / "mcfx_convolver.vst3" / "Contents" / "x86_64-win",
            ]:
                if dst.is_dir():
                    shutil.copy2(dll, dst)


def pip_install() -> None:
    req = REPO_ROOT / "tests" / "requirements.txt"
    run([sys.executable, "-m", "pip", "install", "-r", str(req)])


def pytest_run(extra: list[str]) -> int:
    cmd = [sys.executable, "-m", "pytest", str(REPO_ROOT / "tests")] + extra
    print(f"\n$ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd).returncode


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build prerequisites and run the mcfx pytest suite.",
        epilog="Pass extra pytest args after `--`, "
               "e.g. `python tests/run_tests.py -- -k filter -x`.",
    )
    parser.add_argument("--no-configure", action="store_true",
                        help="Skip the cmake configure step.")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip the cmake build step.")
    parser.add_argument("--no-pip", action="store_true",
                        help="Skip `pip install -r tests/requirements.txt`.")
    parser.add_argument("--no-tests", action="store_true",
                        help="Skip pytest (build-only).")
    args, extra = parser.parse_known_args()

    # argparse leaves the literal "--" separator in extra; strip it.
    if extra and extra[0] == "--":
        extra = extra[1:]

    if not args.no_configure:
        cmake_configure()
    if not args.no_build:
        cmake_build()
    if not args.no_pip:
        pip_install()
    if args.no_tests:
        return 0
    return pytest_run(extra)


if __name__ == "__main__":
    sys.exit(main())
