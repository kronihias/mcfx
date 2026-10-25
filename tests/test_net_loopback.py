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

def test_rtwake_does_not_accumulate_posts():
    """RtWake: signals that land while SendThread is awake must not leave
    extra semaphore posts behind, or later waits stop sleeping and the
    thread spins."""
    _need_binary()
    proc = subprocess.run([LOOPBACK, "--rtwake-test"],
                          capture_output=True, text=True, timeout=30)
    assert proc.returncode == 0, proc.stderr


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
# Host behaviour — events real DAWs produce mid-stream. Each scenario
# reports tail_rms_ch0 / tail_fundamental_hz over the end of the run (a
# 0.5-amplitude sine has RMS ~0.354).
#
# The counter / fast-mode tests assert on stream counters, which separate
# fixed from broken code deterministically; test_sender_stall_recovers
# covers recovery of the audio itself.
# ---------------------------------------------------------------------------

def _assert_tail_audio(m, hz_tol=2.0):
    assert m["tail_rms_ch0"] > 0.3, \
        f"output silent/attenuated at end of run: tail RMS {m['tail_rms_ch0']}"
    assert m["tail_fundamental_hz"] == pytest.approx(1000.0, abs=hz_tol), \
        f"tail pitch wrong: {m['tail_fundamental_hz']} Hz"


def test_reprepare_keeps_frame_count(tmp_path):
    """Host re-runs prepareToPlay mid-stream (Live re-activates plug-ins
    routinely). The sender must keep its frame count running: rewinding
    it under the same UID made the receiver drop every later packet as
    out-of-order, i.e. permanent silence."""
    m = run_loopback("reprepare", 18, tmp_path / "m.json")
    assert m["peer_ever_primed"]
    assert m["dropped_reorder"] == 0, \
        f"receiver dropped {m['dropped_reorder']} packets as reordered after re-prepare"
    assert m["stream_restarts"] == 0, "sender frame count jumped back on re-prepare"


@pytest.mark.parametrize("scenario", ["split_fast_mode", "bunch_fast_mode"])
def test_irregular_calls_leave_fast_mode(scenario, tmp_path):
    """A short pre-render trips the sender's anticipative-render detector;
    afterwards the host runs at realtime but not one call per period:
    split_fast_mode splits every period into 4 back-to-back calls,
    bunch_fast_mode processes 4 full blocks back-to-back per device period.
    Judged call by call (split) or block by block (bunch), the fast calls
    kept the detector in fast mode forever and nothing was sent."""
    m = run_loopback(scenario, 12, tmp_path / "m.json")
    assert m["fast_mode_entries"] >= 1, "burst didn't trip fast mode — scenario not exercised"
    assert not m["fast_mode_active_end"], "sender still discarding audio at end of run"
    # At least one DATA packet per sender period once realtime resumes.
    periods = (m["duration_sec"] - 1.0) * m["sender_sample_rate"] / m["sender_period"]
    assert m["sent_packets"] >= 0.8 * periods, \
        f"sender only sent {m['sent_packets']} packets for ~{int(periods)} periods"


@pytest.mark.parametrize("counter_start", [0, 3_000_000_000])
def test_counter_jump_back_realigns(counter_start, tmp_path):
    """Sender frame count jumps back to 0 under the same UID — an older
    sender re-preparing. The receiver must re-align instead of dropping the
    rest of the stream. From 3e9 the jump back is more than half the 32-bit
    wire range, so it must not be mistaken for a wrap."""
    m = run_loopback("counter_rewind", 18, tmp_path / "m.json",
                     counter_start=counter_start)
    assert m["stream_restarts"] >= 1, "receiver never re-aligned"
    # Without the re-align every packet after the jump (~9000 here) is
    # dropped as reordered; a handful of genuine late packets is fine.
    assert m["dropped_reorder"] <= 10, \
        f"receiver dropped {m['dropped_reorder']} packets after the jump"


# 2^32 - 6 s: the wrap lands ~6 s into the run, on a period boundary or
# (minus 37 frames) in the middle of a packet.
@pytest.mark.parametrize("counter_start", [4294967296 - 288000, 4294967296 - 288000 - 37])
def test_counter_wrap_is_seamless(counter_start, tmp_path):
    """The 32-bit wire frame count wraps (~24.8 h at 48 kHz). The receiver
    extends it to 64 bits, so the stream carries on: no re-align, no
    re-prime, no dropout."""
    m = run_loopback("counter_wrap", 18, tmp_path / "m.json",
                     counter_start=counter_start)
    assert m["stream_restarts"] == 0, "wrap treated as a stream restart"
    assert m["dropped_reorder"] <= 10, \
        f"receiver dropped {m['dropped_reorder']} packets around the wrap"
    assert m["underruns"] <= 2, f"underruns around the wrap: {m['underruns']}"


@pytest.mark.parametrize("stall_ms", [50, 500])
def test_sender_stall_recovers(stall_ms, tmp_path):
    """Host stops calling processBlock for a moment (overloaded machine),
    then carries on at realtime. The receiver should underrun once and
    settle back to clean audio. Before the arrival-time DLL re-anchored on
    timing steps, the step was read as sender clock drift: the rate loop
    pinned at its clamp and anti-windup reset forever (muted / ~5 % sharp)."""
    m = run_loopback("stall", 18, tmp_path / "m.json", stall_ms=stall_ms)
    assert m["underruns"] <= 5, f"underrun cascade after stall: {m['underruns']}"
    # ±15 Hz, not ±2: on a loaded machine extra scheduling underruns can
    # land in the tail window and the loop is mid re-lock (seen up to
    # ~1 %). The failure this guards against is the pin at the ±5 % clamp
    # (~1050 Hz, or muted output), which this still separates cleanly.
    _assert_tail_audio(m, hz_tol=15.0)


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
