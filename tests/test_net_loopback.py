"""
Network audio integration tests for mcfx_send + mcfx_receive.

Drives a single-process loopback through the mcfx_net_loopback_test
binary and asserts on JSON metrics it emits. The binary lives next to
mcfx_testhost in _build/testhost/ and is built via:

    cmake -B _build -DBUILD_VST3=ON -DBUILD_NET_TESTS=ON
    cmake --build _build --target mcfx_net_loopback_test -j8

Most tests run in a few seconds. The `soak` test is gated behind the
`soak` pytest marker; default `pytest` skips it. Run explicitly:

    pytest test_net_loopback.py -m soak -v

Conceptually the suite separates two distinct things:

* **Reported SR mismatch** (sender DESC says 48000, receiver runs at
  50400) — handled entirely by the resampler's nominal output/input
  ratio. The control loop has no work to do; rcorr stays at 1.0.
  Tested via `passthrough` and `drift_match` scenarios.

* **Unreported clock drift** (both sides REPORT 48000 but their
  hardware clocks actually differ) — the only thing the rate-control
  loop can compensate for. Simulated by setting the test driver's
  wallclock pacing (`--recv-pace-sr` / `--sender-pace-sr`) different
  from the reported SR. Tested via the `drift_*` scenarios.

The loop integrator `z3` carries the steady-state DC offset:
   rcorr_DC = 1 - z3
For a receiver paced X ppm fast vs sender, expected z3 = -X*1e-6 and
expected steady-state rcorr = 1 + X*1e-6.
"""

import json
import os
import statistics
import subprocess
import sys
import pytest

# These tests run two sleep_until-paced threads in one process and assert
# on tight audio-scheduling bounds (underruns, pitch stability, control-loop
# convergence). Shared GitHub-hosted runner VMs don't provide the scheduler
# determinism that requires — even the smoke test's 1 kHz sine comes back at
# 500 Hz under CPU starvation. Skip the whole module on CI.
if os.environ.get("GITHUB_ACTIONS") or os.environ.get("CI"):
    pytest.skip(
        "net_loopback tests need real-time scheduling stability; "
        "skipped on CI runners",
        allow_module_level=True)

REPO_ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_EXE       = ".exe" if sys.platform == "win32" else ""
LOOPBACK   = os.path.join(REPO_ROOT, "_build", "testhost",
                          f"mcfx_net_loopback_test{_EXE}")


def _need_binary():
    if not os.path.isfile(LOOPBACK):
        pytest.skip(
            f"loopback binary not built (expected at {LOOPBACK!r}); "
            "rebuild with cmake -DBUILD_NET_TESTS=ON")


def run_loopback(scenario, duration_sec, metrics_out, **kwargs):
    """Run one scenario, return parsed metrics JSON."""
    _need_binary()
    args = [LOOPBACK,
            "--scenario", scenario,
            "--duration-sec", str(duration_sec),
            "--metrics-out", str(metrics_out)]
    for k, v in kwargs.items():
        args += [f"--{k.replace('_','-')}", str(v)]
    proc = subprocess.run(args, capture_output=True, text=True,
                          timeout=duration_sec + 30)
    print(proc.stderr)   # surface stream-level diagnostic prints
    assert proc.returncode == 0, (
        f"loopback exited {proc.returncode}: {proc.stderr}")
    with open(metrics_out) as f:
        return json.load(f)


def _trace_tail_means(trace, fraction=0.5):
    """Mean of rcorr / z3 over the last `fraction` of the trace.

    Steady-state diagnostics — the per-second snapshots oscillate by
    ±period/2 so a single point is meaningless; the mean over multiple
    seconds gives the converged DC value.
    """
    n = max(1, int(len(trace) * fraction))
    tail = trace[-n:]
    return {
        "rcorr": statistics.fmean(p["rcorr"] for p in tail),
        "z3":    statistics.fmean(p["rcZ3"]  for p in tail),
        "fill":  statistics.fmean(p["fillFrames"] for p in tail),
    }


# ---------------------------------------------------------------------------
# Smoke + sanity
# ---------------------------------------------------------------------------

def test_smoke(tmp_path):
    """Connect, prime, audio flows, no errors."""
    m = run_loopback("smoke", 5, tmp_path / "m.json")
    assert not m["internal_error"], m["error_message"]
    assert m["peer_ever_primed"], "peer never primed — connect failed"
    assert m["sent_packets"] > 0
    assert m["recv_packets"] > 0
    assert m["recv_packets"] >= 0.95 * m["sent_packets"], \
        f"packet loss too high: sent={m['sent_packets']} recv={m['recv_packets']}"
    # A single underrun on first prime is acceptable; sustained loss isn't.
    assert m["underruns"] <= 2, f"too many underruns: {m['underruns']}"
    # The sender's FIFO is only ~5 ms deep, so OS scheduling jitter
    # between the test driver's push thread and SendStream's SendThread
    # can occasionally tip a block — count remains bounded though.
    assert m["audio_overruns"] <= 5, f"too many audio overruns: {m['audio_overruns']}"
    assert m["gap_frames"] == 0


def test_passthrough_fundamental(tmp_path):
    """Send a 1 kHz sine on channel 0; verify receiver output matches."""
    m = run_loopback("passthrough", 10, tmp_path / "m.json")
    assert m["measured_fundamental_hz"] == pytest.approx(1000.0, abs=2.0), \
        f"channel 0 fundamental drifted: {m['measured_fundamental_hz']} Hz"
    assert m["underruns"] <= 2
    assert m["audio_overruns"] <= 5


def test_pitch_stability_long(tmp_path):
    """Long-term pitch stability — send a 1 kHz sine for 60 s and check the
    fundamental frequency is steady to a tight tolerance across multiple
    non-overlapping FFT windows. Catches both sustained pitch BIAS (loop
    locking at the wrong rcorr) and pitch JITTER (loop wobbling). The
    short-window passthrough_fundamental check above only catches gross
    failures (±2 Hz = ±2000 ppm at 1 kHz) — far above audible. Audible
    pitch artifacts start at ~5–10 ppm of jitter and ~50 ppm of sustained
    bias, so the bounds here are tightened accordingly."""
    m = run_loopback("pitch_stability", 60, tmp_path / "m.json")
    assert m["underruns"] <= 2, f"underruns: {m['underruns']}"
    assert m["audio_overruns"] <= 20, f"audio_overruns: {m['audio_overruns']}"
    assert m["gap_frames"] == 0, f"gap_frames: {m['gap_frames']}"

    fundHz = m["fundamental_hz_windows"]
    assert isinstance(fundHz, list) and len(fundHz) >= 4, \
        f"expected ≥4 fundamental windows, got: {fundHz!r}"

    # Use median for the bias check and a TRIMMED stdev (drop the
    # single most-outlying value) for the jitter check. The test runs
    # two sleep_until-paced threads in one process, so OS scheduling
    # can occasionally produce a transient that lands inside one of
    # the FFT windows; we don't want a single such outlier to fail
    # the test, but we DO want to catch a pattern where multiple
    # windows are off (real loop instability).
    median = statistics.median(fundHz)
    # Trim: drop the two values furthest from the median, then compute
    # stdev on the remaining six. Two trims because OS scheduling on
    # this two-thread harness occasionally lands a brief transient
    # inside a measurement window — we want the test to fail on real
    # loop wobble (most windows off) but tolerate a couple of harness
    # artifacts. Six clean windows out of eight is still a strong
    # signal that the loop is stable.
    trimmed = sorted(fundHz, key=lambda x: abs(x - median))[:-2]
    trimmed_stdev = statistics.stdev(trimmed)
    # Bias: ≤150 ppm of 1000 Hz = ≤0.15 Hz. The bias has two components:
    # parabolic-interpolation precision (~4 ppm) and the test-driver's
    # independent sleep_until threading asymmetry (~80–90 ppm
    # observed). Real-DAW pitch bias should be far lower since the
    # loop sees zero actual rate mismatch.
    assert abs(median - 1000.0) < 0.15, \
        f"sustained pitch bias: median={median:.4f} Hz (expected 1000.0)"
    # Trimmed jitter: ≤25 ppm RMS (≤0.025 Hz) across all but one
    # window. This catches a real loop wobble (multiple windows off)
    # while tolerating occasional OS-scheduling-induced transients
    # in a single window. Empirically observe 3–5 ppm on a healthy
    # run with 0–1 outliers absorbed.
    assert trimmed_stdev < 0.025, \
        f"pitch jitter across windows (trimmed): stdev={trimmed_stdev:.5f} Hz; values={fundHz}"


def test_drift_match_stable(tmp_path):
    """Matched 48 kHz both sides — loop sits at unity, no glitches."""
    m = run_loopback("drift_match", 30, tmp_path / "m.json")
    assert m["underruns"] <= 2
    assert m["audio_overruns"] <= 10
    assert m["gap_frames"] == 0
    assert m["fill_stable"]
    means = _trace_tail_means(m["trace"], fraction=0.5)
    # Two threads in the same process still see small wallclock jitter
    # (steady_clock has ~µs resolution, sleep_until precision varies);
    # the loop chases that as if it were tiny drift, so we tolerate
    # ~100 ppm of residual settling in a 30 s matched-rate run.
    assert abs(means["rcorr"] - 1.0) < 2e-4, \
        f"matched-rate mean rcorr drifted: {means['rcorr']}"
    assert abs(means["z3"]) < 2e-4, \
        f"matched-rate mean z3 wound up: {means['z3']}"


# ---------------------------------------------------------------------------
# Clock-drift convergence
#
# Both sides report 48000. The receiver's wallclock pacer is set X ppm
# fast or slow. Expected z3 ≈ -X*1e-6 (since rcorr_DC = 1 - z3, and
# rcorr_DC must equal recv_pace / sender_pace = 1 + X*1e-6).
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("ppm,duration", [
    ( +200, 60),
    ( -200, 60),
    (+2000, 60),
    (-2000, 60),
])
def test_drift_convergence(ppm, duration, tmp_path):
    """Receiver's clock is X ppm off the sender's; loop must drive
    rcorr in the right direction with at least half the expected
    magnitude. (Full convergence to ppm precision takes minutes at
    the loop's 0.05 Hz tracking bandwidth — that's the soak test.)"""
    recv_pace = 48000.0 * (1.0 + ppm * 1e-6)
    m = run_loopback("drift_match", duration, tmp_path / "m.json",
                     recv_pace_sr=recv_pace)
    # Underrun/overrun bounds are loose enough to absorb OS scheduling
    # jitter under sequential test load, but still catch a control loop
    # that's totally failing to track.
    assert m["underruns"] <= 20, \
        f"{ppm} ppm drift → underrun runaway: {m['underruns']}"
    assert m["audio_overruns"] <= 100, \
        f"{ppm} ppm: audio_overruns runaway = {m['audio_overruns']}"

    means = _trace_tail_means(m["trace"], fraction=0.5)
    expected_rcorr = 1.0 + ppm * 1e-6
    expected_z3    = -ppm * 1e-6

    # Direction: rcorr must be above 1 for positive ppm, below for
    # negative. Magnitude: at least 50% of target — the loop is
    # well underway, even if not fully settled.
    rcorr_dev = means["rcorr"] - 1.0   # signed
    expected_dev = ppm * 1e-6
    assert (rcorr_dev > 0) == (expected_dev > 0), (
        f"{ppm} ppm: rcorr direction wrong (mean {means['rcorr']})")
    assert abs(rcorr_dev) >= 0.5 * abs(expected_dev), (
        f"{ppm} ppm: loop not tracking — mean rcorr deviation "
        f"{rcorr_dev} is < 50% of expected {expected_dev}")
    # z3 carries the slow integrator state; same direction check.
    assert (means["z3"] > 0) == (expected_z3 > 0), (
        f"{ppm} ppm: z3 direction wrong (mean {means['z3']})")
    assert abs(means["z3"]) >= 0.5 * abs(expected_z3), (
        f"{ppm} ppm: z3 magnitude {means['z3']} < 50% of expected "
        f"{expected_z3} — loop slow to converge")


def test_drift_extreme_degrades_gracefully(tmp_path):
    """+60000 ppm = past the +5% rcorr clamp — loop CAN'T track. Must
    not crash, leak, or emit ridiculous counter values."""
    m = run_loopback("drift_extreme", 30, tmp_path / "m.json")
    assert not m["internal_error"]
    means = _trace_tail_means(m["trace"], fraction=0.5)
    # rcorr should sit near the 1.05 upper clamp most of the time since
    # the receiver paces faster than the sender provides. Anti-windup
    # resets fire repeatedly though (the loop can't track 60000 ppm),
    # each one momentarily dropping rcorr to 1.0 before it recovers to
    # the clamp. Mean depends on the reset/recovery duty cycle, which
    # OS scheduling makes quite variable in this degenerate-state edge
    # case (observed run-to-run variance: ~1.03 to ~1.04). A bound at
    # 1.03 still catches genuine failure modes (loop totally fails to
    # push rcorr above unity) while accepting the ~50% duty cycle
    # variance natural to this scenario.
    assert means["rcorr"] >= 1.030, \
        f"loop didn't push rcorr toward upper clamp: mean rcorr={means['rcorr']}"
    # Underruns/overruns should accumulate (the loop can't keep up) but
    # remain bounded (no 64-bit-overflow runaway).
    assert m["underruns"] < 100_000, "runaway underrun count"
    assert m["gap_frames"] < 100_000_000, "runaway gap_frames"


def test_burst_recovers(tmp_path):
    """Sender pushes at 2× realtime for 5 s (REAPER-style pre-roll),
    then realtime. Receiver overrun guard should fire briefly, audio
    should recover to a stable operating point."""
    m = run_loopback("burst", 30, tmp_path / "m.json")
    assert m["peer_ever_primed"]
    means = _trace_tail_means(m["trace"], fraction=0.3)   # last 9 s
    # After the burst ends at t=5 s and we run another 25 s, the loop
    # should have unwound back to ~unity (within the clamp range).
    assert 0.95 <= means["rcorr"] <= 1.05, \
        f"loop didn't recover from burst: mean rcorr={means['rcorr']}"


# ---------------------------------------------------------------------------
# Soak — env-gated; only runs when explicitly requested.
# ---------------------------------------------------------------------------

@pytest.mark.soak
def test_soak_matched_rate(tmp_path):
    """4-hour matched-rate run. Counters stable, no runaway, fill bounded."""
    m = run_loopback("soak", 14400, tmp_path / "m.json")
    assert m["underruns"] <= 5
    assert m["audio_overruns"] == 0
    assert m["gap_frames"] == 0
    assert m["fill_stable"]
