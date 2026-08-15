"""Run every built VST3 through Tracktion's pluginval.

https://github.com/Tracktion/pluginval

Covers what the rest of the suite structurally cannot: the plug-in
*lifecycle*. The other tests drive plug-ins through one stable
configuration, while pluginval hammers repeated prepare/release cycles,
changing sample rates and block sizes, bus-layout permutations, state
round-trips and parameter fuzzing. The crash that got mcfx_delay rejected
by hosts that validate before loading (a stale ring write position after
a re-prepare at a lower sample rate) reproduces as a segfault in
pluginval's Automation test, and would have been caught here.

pluginval is optional: without it the tests skip, like the rest of the
suite does for missing tools. Point PLUGINVAL at the binary, or install
it where the platform lookup below finds it.

  PLUGINVAL=/path/to/pluginval pytest tests/test_pluginval.py
  PLUGINVAL_STRICTNESS=5 pytest tests/test_pluginval.py    # default 10
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))

from conftest import VST3_DIR, vst3_load_path

# Strictness 10 is pluginval's maximum; 5 is its recommended minimum. Every
# mcfx plug-in passes at 10, so there is no reason to ask for less.
STRICTNESS = os.environ.get("PLUGINVAL_STRICTNESS", "10")

# pluginval reports failures in its output but does not reliably exit
# non-zero — a segfaulting plug-in still gave exit 0 — so the output is
# what we assert on.
FAILURE_MARKERS = ("segmentation fault", "failed", "*** fail", "bus error")


def _find_pluginval() -> str | None:
    if (env := os.environ.get("PLUGINVAL")):
        return env if os.path.exists(env) else None

    if found := shutil.which("pluginval"):
        return found

    candidates = [
        "/Applications/pluginval.app/Contents/MacOS/pluginval",
        os.path.expanduser("~/Applications/pluginval.app/Contents/MacOS/pluginval"),
        "C:/Program Files/pluginval/pluginval.exe",
    ]
    return next((c for c in candidates if os.path.exists(c)), None)


PLUGINVAL = _find_pluginval()

pytestmark = pytest.mark.skipif(
    PLUGINVAL is None,
    reason="pluginval not found — set PLUGINVAL to its path "
           "(https://github.com/Tracktion/pluginval)",
)


def _built_plugins() -> list[str]:
    if not os.path.isdir(VST3_DIR):
        return []
    return sorted(f for f in os.listdir(VST3_DIR) if f.endswith(".vst3"))


@pytest.mark.parametrize("plugin_name", _built_plugins() or ["<none built>"])
def test_pluginval_clean(plugin_name):
    if plugin_name == "<none built>":
        pytest.skip(f"No plug-ins built in {VST3_DIR}")

    path = vst3_load_path(os.path.join(VST3_DIR, plugin_name))

    proc = subprocess.run(
        [PLUGINVAL, "--strictness-level", STRICTNESS, "--validate", path],
        capture_output=True, text=True, timeout=600,
    )
    output = proc.stdout + proc.stderr
    lowered = output.lower()

    hits = [line for line in output.splitlines()
            if any(m in line.lower() for m in FAILURE_MARKERS)]

    assert not hits, (
        f"{plugin_name} failed pluginval (strictness {STRICTNESS}):\n"
        + "\n".join(hits[:10])
        + f"\n--- last output ---\n" + "\n".join(output.splitlines()[-15:])
    )

    # A run that never reached the end means pluginval died partway through
    # (that is how the mcfx_delay crash showed up), which the marker scan
    # above can miss if the crash message goes only to the crash reporter.
    assert "all tests completed successfully" in lowered or proc.returncode == 0, (
        f"{plugin_name}: pluginval did not finish cleanly "
        f"(exit {proc.returncode}):\n" + "\n".join(output.splitlines()[-15:])
    )
