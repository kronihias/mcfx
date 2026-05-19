"""
mcfx_anything parameter-sync test.

mcfx_anything is a meta-plugin: it loads any VST/AU and runs N synchronised
instances (one per channel pair). The sync invariant we verify here:

  Send identical white noise on every input channel pair, set a non-default
  forwarded parameter (mapped to the inner plugin's first user-facing
  parameter), then process. Every slave instance must apply the same DSP, so:

    1. output != input  (the parameter actually took effect)
    2. output[ch_a, :] == output[ch_b, :] for every pair of channels at the
       same position within their respective slave instances  (proof that
       all slaves received the same parameter value)

This requires testhost's --describe-plugin and --load-inner-state flags,
which build the mcfx_anything state blob (containing a PluginDescription
pointing at the chosen inner plugin) without re-implementing JUCE's plugin
scanner in Python.
"""

from __future__ import annotations

import os
import re
import struct
import subprocess

import numpy as np
import pytest

from conftest import SR, TESTHOST_BIN, vst3

pytestmark = pytest.mark.skipif(
    not os.path.exists(TESTHOST_BIN),
    reason=f"mcfx_testhost not built: {TESTHOST_BIN}",
)

ANYTHING_VST3 = vst3("mcfx_anything")
FILTER_VST3   = vst3("mcfx_filter")

# JUCE binary header magic for copyXmlToBinary: 'VC2!' + little-endian length.
JUCE_XML_MAGIC = b"VC2!"


def _juce_xml_to_binary(xml_string: str) -> bytes:
    """Replicates AudioProcessor::copyXmlToBinary: VC2! header + len + UTF-8."""
    payload = xml_string.encode("utf-8")
    return JUCE_XML_MAGIC + struct.pack("<I", len(payload)) + payload


def _describe_plugin(plugin_path: str) -> str:
    """Use testhost to extract a plugin's <PLUGIN ...> description XML."""
    out = subprocess.run(
        [TESTHOST_BIN, "--describe-plugin", plugin_path],
        check=True, capture_output=True, text=True,
    ).stdout
    m = re.search(r"<PLUGIN\b[^>]*/?>", out, re.DOTALL)
    if m is None:
        raise RuntimeError(f"No <PLUGIN .../> element in describe output:\n{out}")
    return m.group(0)


def _build_anything_state(inner_plugin_path: str) -> bytes:
    """Construct the MCFX_ANYTHING_STATE blob in JUCE binary form."""
    desc = _describe_plugin(inner_plugin_path)
    xml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<MCFX_ANYTHING_STATE>'
        '<CURRENT_PLUGIN>' + desc + '</CURRENT_PLUGIN>'
        '</MCFX_ANYTHING_STATE>'
    )
    return _juce_xml_to_binary(xml)


def test_anything_master_param_syncs_to_slaves(tmp_path):
    """Set a non-default forwarded param; all slave channel pairs must apply it."""
    pytest.importorskip("soundfile")
    import soundfile as sf
    import json

    if not os.path.exists(ANYTHING_VST3):
        pytest.skip(f"mcfx_anything not built: {ANYTHING_VST3}")
    if not os.path.exists(FILTER_VST3):
        pytest.skip(f"mcfx_filter not built: {FILTER_VST3}")

    n_ch = 8                # 4 slave instances of mcfx_filter
    n_smp = SR // 2         # 0.5 s

    # Reproducible noise, replicated identically on every channel
    rng = np.random.default_rng(123)
    mono = rng.standard_normal(n_smp).astype(np.float32)
    audio = np.tile(mono, (n_ch, 1))

    in_wav  = str(tmp_path / "in.wav")
    out_wav = str(tmp_path / "out.wav")
    state   = str(tmp_path / "anything_state.bin")
    params  = str(tmp_path / "params.json")

    sf.write(in_wav, audio.T, SR, subtype="FLOAT")
    open(state, "wb").write(_build_anything_state(FILTER_VST3))

    # "Param 1" on mcfx_anything is the first forwarded slot, bound to the
    # inner plugin's first parameter. For mcfx_filter that's "LowCut On".
    # Setting it to 1.0 engages a Butterworth high-pass on every slave —
    # output spectra below the default cutoff drop, and all slave pairs must
    # show the same drop if sync is working.
    json.dump({"Param 1": 1.0}, open(params, "w"))

    subprocess.run([
        TESTHOST_BIN,
        "--plugin",            ANYTHING_VST3,
        "--load-inner-state",  state,
        "--params",            params,
        "--input",             in_wav,
        "--output",            out_wav,
        "--channels",          str(n_ch),
    ], check=True, capture_output=True)

    out, _ = sf.read(out_wav, dtype="float32")
    out = out.T  # → (n_ch, n_smp)
    assert out.shape == (n_ch, n_smp), f"Unexpected output shape: {out.shape}"

    # Drop the first 4096 samples (param-smoothing ramp + filter transient).
    tail = out[:, 4096:]
    inp  = audio[:, 4096:]

    # Sanity 1: the parameter actually engaged a filter on at least one slave.
    rms_in  = float(np.sqrt(np.mean(inp[0]**2)))
    rms_out = float(np.sqrt(np.mean(tail[0]**2)))
    assert rms_in > 0
    assert abs(rms_out - rms_in) / rms_in > 1e-3, (
        "Output RMS unchanged from input — the forwarded parameter did "
        "not take effect (slave didn't apply LowCut, or sync didn't fire)."
    )

    # Sanity 2: every slave produced the same output as slave 0.
    # mcfx_anything pairs channels (0,1), (2,3), ... Each slave receives the
    # same identical noise mix, so each slave's two output channels must equal
    # slave 0's two output channels.
    for slave_idx in range(1, n_ch // 2):
        a_ch = slave_idx * 2
        b_ch = slave_idx * 2 + 1
        np.testing.assert_allclose(
            tail[a_ch], tail[0], atol=1e-5,
            err_msg=f"Slave {slave_idx} ch0 diverges from slave 0 ch0",
        )
        np.testing.assert_allclose(
            tail[b_ch], tail[1], atol=1e-5,
            err_msg=f"Slave {slave_idx} ch1 diverges from slave 0 ch1",
        )
