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
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "_build"
IS_WINDOWS = sys.platform == "win32"
IS_MACOS   = sys.platform == "darwin"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print(f"\n$ {' '.join(str(c) for c in cmd)}", flush=True)
    subprocess.run(cmd, cwd=cwd, check=True)


def ensure_vcpkg_baseline(vcpkg_root: Path) -> None:
    # The preinstalled vcpkg on GH Actions runners often predates the
    # builtin-baseline pinned in vcpkg.json, and `vcpkg install` then aborts
    # with "failed to git show versions/baseline.json". Fetch the missing
    # commit before invoking cmake.
    manifest = REPO_ROOT / "vcpkg.json"
    try:
        baseline = json.loads(manifest.read_text())["builtin-baseline"]
    except (FileNotFoundError, KeyError, json.JSONDecodeError):
        return
    if not (vcpkg_root / ".git").exists():
        return
    have = subprocess.run(
        ["git", "-C", str(vcpkg_root), "cat-file", "-e", f"{baseline}^{{commit}}"],
        capture_output=True,
    )
    if have.returncode == 0:
        return
    print(f"\n[vcpkg] baseline {baseline[:12]} missing in {vcpkg_root}, fetching", flush=True)
    fetch = subprocess.run(
        ["git", "-C", str(vcpkg_root), "fetch", "--depth", "1", "origin", baseline],
    )
    if fetch.returncode != 0:
        # Fetch-by-sha can be refused (uploadpack.allowReachableSHA1InWant);
        # fall back to a full fetch from origin.
        run(["git", "-C", str(vcpkg_root), "fetch", "origin"])


def ensure_vcpkg_root() -> Path:
    # Resolve a usable vcpkg checkout. Honors VCPKG_ROOT, then GH-Actions'
    # VCPKG_INSTALLATION_ROOT; otherwise clones + bootstraps into
    # ~/vcpkg. FFTW3 (manifest-declared in vcpkg.json) is required on
    # Windows since the prebuilt fallback under win-libs/ was removed.
    for env_var in ("VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT"):
        v = os.environ.get(env_var)
        if v and (Path(v) / "scripts" / "buildsystems" / "vcpkg.cmake").is_file():
            return Path(v)

    home_vcpkg = Path.home() / "vcpkg"
    if not (home_vcpkg / "scripts" / "buildsystems" / "vcpkg.cmake").is_file():
        print(f"\n[vcpkg] bootstrapping into {home_vcpkg}", flush=True)
        if not home_vcpkg.exists():
            run(["git", "clone", "https://github.com/microsoft/vcpkg", str(home_vcpkg)])
        bootstrap = home_vcpkg / ("bootstrap-vcpkg.bat" if IS_WINDOWS else "bootstrap-vcpkg.sh")
        run([str(bootstrap), "-disableMetrics"])
    return home_vcpkg


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
        # FFTW3 comes from vcpkg manifest mode (see vcpkg.json — static lib
        # with SSE2/AVX/AVX2/threads). GH Actions runners expose
        # VCPKG_INSTALLATION_ROOT; local dev boxes set VCPKG_ROOT; otherwise
        # we bootstrap into ~/vcpkg.
        vcpkg_root = ensure_vcpkg_root()
        ensure_vcpkg_baseline(vcpkg_root)
        # In-tree overlay triplet so vcpkg only builds the Release variant of
        # FFTW3 (we don't link a debug fftw3f.lib). Halves the first-time
        # vcpkg build cost.
        overlay = (REPO_ROOT / "vcpkg-triplets").as_posix()
        args += [
            f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_root.as_posix()}/scripts/buildsystems/vcpkg.cmake",
            f"-DVCPKG_OVERLAY_TRIPLETS={overlay}",
            "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md-release",
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
