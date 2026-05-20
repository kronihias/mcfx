"""
Smoke test: every built mcfx VST3 loads, accepts an impulse, and produces
finite output of the expected shape.

Cheap safety net for plugins without dedicated DSP tests (mcfx_meter,
mcfx_graph, mcfx_anything), and a regression guard for the rest if a
refactor breaks plugin startup or basic processing.
"""

import glob
import os

import numpy as np
import pytest

from conftest import VST3_DIR, SR, vst3_load_path

PLUGIN_PATHS = sorted(glob.glob(os.path.join(VST3_DIR, "mcfx_*.vst3")))


@pytest.mark.parametrize("plugin_path", PLUGIN_PATHS,
                         ids=[os.path.basename(p) for p in PLUGIN_PATHS])
def test_plugin_loads_and_processes_impulse(plugin_path):
    """Open the plugin, push 1 s of impulse-on-ch0 through it, expect
    finite output with the same shape we sent in."""
    try:
        import pedalboard
    except ImportError:
        pytest.skip("pedalboard not installed")

    if not PLUGIN_PATHS:
        pytest.skip(f"No built mcfx VST3 plugins found in {VST3_DIR}")

    plugin = pedalboard.load_plugin(vst3_load_path(plugin_path))

    audio = np.zeros((2, SR), dtype=np.float32)
    audio[0, 0] = 1.0

    out = plugin(audio, SR)

    assert out.shape[0] == 2, f"Expected 2 output channels, got {out.shape[0]}"
    assert out.shape[1] == SR, f"Expected {SR} samples, got {out.shape[1]}"
    assert np.all(np.isfinite(out)), "Output contains NaN or Inf"
