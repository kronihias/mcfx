"""
mcfx_graph state round-trip via mcfx_testhost.

Property under test: feeding the bytes from getStateInformation back through
setStateInformation must re-emit identical bytes (no information loss in the
load/save path). Uses the --save-state / --load-state flags on mcfx_testhost.

Requires:
  - mcfx_graph.vst3 built
  - mcfx_testhost built with --save-state / --load-state
  - soundfile python package
"""

from __future__ import annotations

import os
import subprocess

import numpy as np
import pytest

from conftest import SR, TESTHOST_BIN, vst3

pytestmark = pytest.mark.skipif(
    not os.path.exists(TESTHOST_BIN),
    reason=f"mcfx_testhost not built: {TESTHOST_BIN}",
)

GRAPH_VST3 = vst3("mcfx_graph")


def _silence_wav(path: str, n_ch: int = 2, n_samples: int = SR // 4) -> None:
    pytest.importorskip("soundfile")
    import soundfile as sf
    sf.write(path, np.zeros((n_samples, n_ch), dtype=np.float32),
             SR, subtype="FLOAT")


def _run_testhost(*, input_wav: str, save_state: str,
                  load_state=None) -> None:
    cmd = [
        TESTHOST_BIN,
        "--plugin",     GRAPH_VST3,
        "--input",      input_wav,
        "--no-output",
        "--save-state", save_state,
    ]
    if load_state is not None:
        cmd += ["--load-state", load_state]
    subprocess.run(cmd, check=True, capture_output=True)


def test_graph_default_state_roundtrip(tmp_path):
    """getStateInformation → setStateInformation → getStateInformation must
    produce identical bytes."""
    if not os.path.exists(GRAPH_VST3):
        pytest.skip(f"mcfx_graph not built: {GRAPH_VST3}")

    silence = str(tmp_path / "silence.wav")
    state_a = str(tmp_path / "state_a.bin")
    state_b = str(tmp_path / "state_b.bin")
    _silence_wav(silence)

    # Run 1: default state (no --load-state), save it.
    _run_testhost(input_wav=silence, save_state=state_a)

    # Run 2: load state_a, save again.
    _run_testhost(input_wav=silence, load_state=state_a, save_state=state_b)

    bytes_a = open(state_a, "rb").read()
    bytes_b = open(state_b, "rb").read()

    # Sanity: state contains the mcfx_graph XML signature, with real payload
    # (the JUCE base64-encoded graph JSON adds significantly to the size).
    assert b"VST3PluginState" in bytes_a, (
        "Saved state is not a VST3 plugin state blob"
    )
    assert len(bytes_a) > 256, (
        f"State is suspiciously small ({len(bytes_a)} bytes) — graph "
        "serializer may not be emitting topology JSON"
    )

    assert bytes_a == bytes_b, (
        "Graph state round-trip changed the saved bytes. "
        f"len_a={len(bytes_a)} len_b={len(bytes_b)}; "
        f"first-diff offset {next((i for i in range(min(len(bytes_a),len(bytes_b))) if bytes_a[i] != bytes_b[i]), 'none')}"
    )
