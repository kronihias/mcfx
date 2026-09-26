"""
mcfx_graph "Convert to subgraph", via the mcfx_graph_test harness.

Each scenario builds a graph twice, converts a selection to a subgraph in one
copy, and requires sample-identical output: fan-out and summing on both sides
of the boundary, a feedback pair moving whole, nesting a second level, and
refusals (split feedback pair, terminals, empty) that change nothing.
Also: changing a subgraph's channel count keeps its inner graph, and
Option/Alt-drag insertion (dropping a node on a wire) sounds like wiring it in by
hand, takes only as many wires as the node has channels, and refuses nodes
that are already wired. And delay compensation: a dry path lines up with a
latent one, also through a subgraph and after a latency change at runtime,
and a node set to ignore its latency is left out (and that is saved).

Requires mcfx_graph_test (cmake -DBUILD_GRAPH_TESTS=ON; run_tests.py does).
"""

from __future__ import annotations

import os
import subprocess

import pytest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_EXE = ".exe" if os.name == "nt" else ""
GRAPH_TEST_BIN = os.path.join(REPO_ROOT, "_build", "testhost", f"mcfx_graph_test{_EXE}")

pytestmark = pytest.mark.skipif(
    not os.path.exists(GRAPH_TEST_BIN),
    reason=f"mcfx_graph_test not built: {GRAPH_TEST_BIN}",
)


@pytest.mark.parametrize("scenario", ["mixed", "isolated", "feedback", "refusals", "nested",
                                      "resize", "insert", "insert-partial", "insert-refusals",
                                      "removal-notices", "latency-parallel", "latency-subgraph",
                                      "latency-runtime", "latency-ignored"])
def test_graph_editing(scenario):
    proc = subprocess.run([GRAPH_TEST_BIN, "--scenario", scenario],
                          capture_output=True, text=True, timeout=120)
    assert proc.returncode == 0, proc.stdout + proc.stderr
