"""
mcfx_graph feedback send/return pair, end to end through mcfx_testhost.

The pair exists because juce::AudioProcessorGraph cannot render a cycle: a
wire from the send back to the return would close one, RenderSequenceBuilder
would fail to find a buffer for the back edge and substitute the read-only
empty buffer, and the loop would carry silence. The pair is deliberately NOT
connected in the graph, so the graph stays acyclic and the loop closes through
a shared double-buffered FeedbackBus instead.

That buys exactly one block of delay, and "exactly one block" is the property
worth pinning down — it is what the whole design rests on, it is invisible in
the editor, and it silently changes with the host's block size. So the tests
below assert the delay in samples at two different block sizes.

Requires:
  - mcfx_graph.vst3 built
  - mcfx_testhost built
  - soundfile python package
"""

from __future__ import annotations

import json
import os
import struct
import subprocess
import xml.sax.saxutils as saxutils

import numpy as np
import pytest

from conftest import SR, TESTHOST_BIN, vst3

pytestmark = pytest.mark.skipif(
    not os.path.exists(TESTHOST_BIN),
    reason=f"mcfx_testhost not built: {TESTHOST_BIN}",
)

GRAPH_VST3 = vst3("mcfx_graph")

# GraphSerializer.h
INPUT_SENTINEL = "__input__"
OUTPUT_SENTINEL = "__output__"
GRAPH_VERSION = 1

# PluginProcessor.cpp
STATE_XML_TAG = "MCFX_GRAPH_STATE"
STATE_JSON_ATTR = "json"

# juce_AudioProcessor.cpp
MAGIC_XML_NUMBER = 0x21324356

SEND_UUID = "11111111-1111-1111-1111-111111111111"
RETURN_UUID = "22222222-2222-2222-2222-222222222222"


def _state_blob(graph_json: dict) -> bytes:
    """Reproduce AudioProcessor::copyXmlToBinary for mcfx_graph's state.

    Layout is magic int, then the byte length of the XML text (excluding the
    9-byte header and trailing NUL), then the single-line XML, then a NUL.
    """
    xml = '<{tag} {attr}="{payload}"/>'.format(
        tag=STATE_XML_TAG,
        attr=STATE_JSON_ATTR,
        payload=saxutils.escape(json.dumps(graph_json), {'"': "&quot;"}),
    ).encode("utf-8")
    return struct.pack("<II", MAGIC_XML_NUMBER, len(xml)) + xml + b"\x00"


def _feedback_graph(n_ch: int) -> dict:
    """input -> feedback send, feedback return -> output, the two linked.

    Nothing connects the send to the return, which is the point: the only path
    from input to output runs through the shared bus, so anything that arrives
    at the output has been round-tripped through the pair.
    """
    return {
        "format": "mcfx_graph",
        "version": GRAPH_VERSION,
        "rootGraph": {
            "nodes": [
                {
                    "uuid": SEND_UUID,
                    "type": "feedback_send",
                    "displayName": "Fb Send",
                    "position": {"x": 400, "y": 100},
                    "channelCountIn": n_ch,
                    "channelCountOut": 0,
                },
                {
                    "uuid": RETURN_UUID,
                    "type": "feedback_return",
                    "displayName": "Fb Return",
                    "position": {"x": 400, "y": 300},
                    "channelCountIn": 0,
                    "channelCountOut": n_ch,
                },
            ],
            "connections": [
                {
                    "from": {"uuid": INPUT_SENTINEL, "channel": c},
                    "to": {"uuid": SEND_UUID, "channel": c},
                }
                for c in range(n_ch)
            ]
            + [
                {
                    "from": {"uuid": RETURN_UUID, "channel": c},
                    "to": {"uuid": OUTPUT_SENTINEL, "channel": c},
                }
                for c in range(n_ch)
            ],
            "feedbackLinks": [{"send": SEND_UUID, "return": RETURN_UUID}],
        },
    }


def _impulse_wav(path: str, n_ch: int, n_samples: int, at: int = 0) -> None:
    sf = pytest.importorskip("soundfile")
    buf = np.zeros((n_samples, n_ch), dtype=np.float32)
    buf[at, :] = 1.0
    sf.write(path, buf, SR, subtype="FLOAT")


def _render(tmp_path, graph_json: dict, block_size: int,
            n_ch: int, n_samples: int) -> np.ndarray:
    sf = pytest.importorskip("soundfile")

    in_wav = str(tmp_path / "in.wav")
    out_wav = str(tmp_path / "out.wav")
    state = str(tmp_path / "state.bin")

    _impulse_wav(in_wav, n_ch, n_samples)
    with open(state, "wb") as fh:
        fh.write(_state_blob(graph_json))

    subprocess.run(
        [
            TESTHOST_BIN,
            "--plugin", GRAPH_VST3,
            "--input", in_wav,
            "--output", out_wav,
            "--channels", str(n_ch),
            "--blocksize", str(block_size),
            "--samplerate", str(SR),
            "--load-inner-state", state,
            # The pair reports zero latency on purpose — the delay is the
            # feature, not something to compensate away. Ask for raw output so
            # the host cannot quietly shift it back and hide a regression.
            "--no-latency-compensation",
        ],
        check=True,
        capture_output=True,
    )

    data, _ = sf.read(out_wav, dtype="float32", always_2d=True)
    return data


@pytest.mark.skipif(not os.path.exists(GRAPH_VST3),
                    reason=f"not built: {GRAPH_VST3}")
@pytest.mark.parametrize("block_size", [128, 512])
def test_feedback_pair_delays_by_exactly_one_block(tmp_path, block_size):
    """An impulse at sample 0 must reappear at sample `block_size`.

    Earlier would mean the send and return were scheduled in an order that let
    audio through within a block — the thing the double buffer exists to make
    impossible. Later (or never) would mean the bus is not carrying at all,
    which is what a plain cyclic wire does today.
    """
    n_ch = 2
    n_samples = block_size * 4
    out = _render(tmp_path, _feedback_graph(n_ch), block_size, n_ch, n_samples)

    assert out.shape[0] >= block_size + 1

    peak_at = int(np.argmax(np.abs(out[:, 0])))
    assert peak_at == block_size, (
        f"impulse landed at {peak_at}, expected {block_size} "
        f"(one block of delay)"
    )
    assert out[block_size, 0] == pytest.approx(1.0, abs=1e-5)

    # Nothing at all before the delay: not a leaked in-block path, not a
    # fragment of the graph's shared scratch buffer.
    assert np.max(np.abs(out[:block_size, :])) < 1e-6


@pytest.mark.skipif(not os.path.exists(GRAPH_VST3),
                    reason=f"not built: {GRAPH_VST3}")
def test_feedback_pair_passes_every_channel(tmp_path):
    """All channels round-trip, not just the first."""
    n_ch = 8
    block_size = 256
    out = _render(tmp_path, _feedback_graph(n_ch), block_size,
                  n_ch, block_size * 3)

    for c in range(n_ch):
        assert out[block_size, c] == pytest.approx(1.0, abs=1e-5), \
            f"channel {c} did not come back"


@pytest.mark.skipif(not os.path.exists(GRAPH_VST3),
                    reason=f"not built: {GRAPH_VST3}")
def test_unlinked_return_emits_silence(tmp_path):
    """A return with no send must be silent, not leak the graph's scratch.

    The return is 0-in/N-out, so the graph may well hand it a buffer another
    node just wrote. Emitting that would be an audible bug in a patch the user
    has not even finished wiring yet.
    """
    n_ch = 2
    block_size = 256
    graph = _feedback_graph(n_ch)
    graph["rootGraph"]["feedbackLinks"] = []

    out = _render(tmp_path, graph, block_size, n_ch, block_size * 3)
    assert np.max(np.abs(out)) < 1e-6, "unlinked return emitted something"
