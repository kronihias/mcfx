# mcfx_receive

Multichannel UDP audio receiver for the mcfx suite. Listens on a UDP
port, decodes the mcfx_send wire protocol, and mixes one or more
incoming streams into the output bus.

## What it does

- Binds an ephemeral UDP port automatically (the port is persisted in the
  plug-in state and re-requested on reload); reports the bound port and
  this machine's IPv4 addresses in the UI.
- Per packet: parses the mcfx wire header, looks up the sender by UID,
  routes the audio frames into that sender's per-peer ring buffer.
- Supports PCM 16-bit, PCM 24-bit, and PCM 32-bit float on the wire.
  Decodes int16/int24 to float at NetThread time; the audio thread
  always reads float frames.
- On audio callback: pulls `blockSize` frames from each primed peer
  through a per-peer variable-ratio sinc resampler and sums into the
  output bus. The resampler ratio is nudged each block by a control
  loop so the receiver stays phase-locked to the sender even when the
  two hardware audio clocks don't agree exactly.
- Adaptive jitter buffer: user-set floor in ms + per-peer auto extra
  that grows on packet loss / underrun and decays slowly during clean
  periods. Bounded at 500 ms total per peer.
- Stale peer eviction: a peer that goes silent for >15 s is dropped.
- Bumps `SO_RCVBUF` to 4 MB and applies QoS markings (matches mcfx_send).
- Publishes itself as `mcfx_receive.v1` so senders on the same subnet see
  it in their browse list, and browses `mcfx_send.v1` so the receiver UI
  can show the sender's-eye view too.
- Outputs silence during anticipative / offline rendering: sustained
  faster-than-realtime `processBlock` calls (REAPER's anticipative FX)
  are detected and the rate loop / DLL state is left untouched until the
  host returns to realtime — future audio doesn't exist yet.

## Connection model

Symmetric. Audio only flows for senders we've explicitly accepted via
the INVITE handshake; inbound DATA from any other UID is dropped with a
DBG log. Three ways to get a sender into the accepted set:

1. **Click a discovered sender** (Local network tab) — the receiver sends
   an INVITE to that sender. The sender password-checks, ACKs, adds us to
   its targets list, and starts streaming. Our INVITE_ACK callback
   marks the entry Active so DATA packets pass the filter.
2. **Inbound INVITE** (sender clicks our advertised entry, or types our
   address manually) — the sender sends us an INVITE; we password-check,
   ACK, and add the sender to our accepted set as Active immediately.
3. **Click Disconnect** in the list — sends UNINVITE, drops the accepted
   entry and the peer's ring buffer (so there's no garbage tail).

Rejected / timed-out entries stay visible for 30 s so the failure is
readable, but only Active entries pass the audio filter.

The accepted set is persisted across reload via the auto-reconnect
mechanism (below); the password is persisted too.

## Discovery

Not mDNS/Bonjour — JUCE's `NetworkServiceDiscovery`, i.e. periodic UDP
broadcast on the local subnet. Receivers advertise `mcfx_receive.v1` on
broadcast port **35518**, senders advertise `mcfx_send.v1` on **35517**
(separate ports because macOS delivers each broadcast to only one
listener per port, and a sender + receiver often share one DAW process).
TXT fields: `uid`, `track`, `host`, `user`, `project`, `ch`, `fmt`, and
`wuid` — the 32-bit wire UID each side writes into `CommonHeader.sender`,
which is what the editors pair rows on. See
[mcfx_send/README.md](../mcfx_send/README.md#discovery) for the full
description; anything broadcast can't reach is still reachable via the
Direct IP tab.

## UI

- **Jitter floor (ms)** — slider, 0…500 ms, default 25. Minimum buffer
  the receiver waits for before starting playback for a peer. Adaptive
  auto-extra adds on top per-peer in response to actual loss; the
  per-peer status line shows the current effective value.
- **Password** — gates inbound INVITEs and is sent in our outbound
  INVITEs. Empty = no auth. Persisted.
- **Auto-reconnect** — toggle, persisted. Senders that were connected at
  save time are re-invited for up to 60 s after the project reopens, with
  1 Hz retries and re-matching by discovery identity if the sender came
  back at a different address. Any Connect / Disconnect click cancels it.
  Same mechanism as
  [mcfx_send](../mcfx_send/README.md#auto-reconnect).
- **Listening on UDP port N** — the bound port plus this machine's IPv4
  addresses, so the user can hand the address to a sender peer who has to
  type it into Direct IP. Read-only (no UI field changes the port).
- **Status banner** — outcome of the most recent INVITE attempt
  ("Inviting …", "Connected to …", "REJECTED - passwords don't match",
  "No response from …"), "Connection healthy" once a peer is Active, or
  the auto-reconnect countdown, which takes precedence.
- **Local network** tab — discovered senders, plus any accepted senders
  with no discovery match (Direct-IP path or a sender that's gone from
  the network but the connection is still up). A filter box matches on
  host / project / track / IP. Status dot per row: gray = discovered/
  disconnected, yellow = inviting, green = connected, red = rejected
  (e.g. bad password), orange = no response. Double-click a row, or use
  the Connect/Disconnect buttons, to toggle.
- **Direct IP** tab — type a sender's IP and listening UDP port to
  connect manually. The resulting connection shows up in the same
  unified list with a "(direct)" host label.
- **Peers** line — one entry per accepted sender: channel count, source
  address, and "(priming)" while the ring is still filling.
- **Network** line — per peer: rate (B/s), packet rate (pps), current
  effective jitter buffer, total estimated one-way latency
  (`lat ~Xms`), ring fill, and counters for gap, underrun, reorderDrop.
- **Audio meters** — per-channel output level meters along the bottom.

## Latency

`lat ~Xms` per peer = sender block + effective jitter buffer + receiver
block + resampler group delay (≈ taps/2 input frames at the sender's
rate; defaults to 80 taps so ~0.83 ms at 48 kHz). Doesn't include
actual network transit (would need NTP-grade synced clocks); on a LAN
it's negligible vs. the buffer + block components. The sender block
comes from the DESC packet's reported period and sample rate.

## Adaptive jitter buffer (the algorithm)

Each peer carries an `autoExtraMs` value added to the user-set floor;
`floor + autoExtra` is clamped to 500 ms and converted into the prefill
frame target (never below one sender period):

- On `gap` (NetThread sees the data sequence skip) or an anti-windup
  reset of the rate loop: bump `autoExtraMs` by `max(10, autoExtraMs/2)`,
  capped at 200. Sets the last-loss timestamp.
- On `underrun` (audio thread couldn't fill the block): jump straight to
  the 200 ms cap and drop the peer back into "priming" mode. Geometric
  growth would need ~6 audible glitches to get there; recovery from a
  genuinely too-small buffer should be a single event.
- Events within ~1 s of a (re)prime don't bump at all — that window is
  the rate loop's own convergence transient, and it's muted anyway.
- Each audio block, if there's been no loss for 60 s, decay
  `autoExtraMs` by 1 ms, at most once per second (≈ 60 ms earned back
  per clean minute).

Fast to react, deliberately slow to give latency back: one bump keeps the
buffer larger for a long while, on the assumption the cause recurs.

## Adaptive sample-rate conversion

Sender and receiver run on independent audio clocks. Even when the
two devices report the same nominal rate (e.g. both 48000 Hz), the
physical crystals drift a few ppm — enough that, over minutes, a
fixed-rate read would either starve or overflow the ring. To stay
phase-locked we run each peer's audio through a per-peer variable-
ratio sinc resampler (vendored
[dbry/audio-resampler](https://github.com/dbry/audio-resampler), BSD-3,
under [common/third_party/audio-resampler](../common/third_party/audio-resampler)).

### Arrival-time DLL

Feeding the rate loop the raw ring fill can't separate slow clock drift
(signal) from network jitter (noise) — on a real LAN the jitter dominates
and produces audible pitch wobble plus anti-windup glitches. So the
NetThread runs a 2nd-order PLL/DLL on packet arrival times, updated once
per sender period (on `HF_LAST_OF_PERIOD` packets), bandwidth 0.05 Hz,
with a ±1-period anti-jump clamp so a lost flagged packet doesn't throw
it. Its state (filtered arrival time, filtered period duration) is
published through an SPSC seqlock pair; the audio thread snapshots it
once per block and interpolates between the two most recent anchors to
get a smooth predicted write position.

The per-packet `tx_us` from the wire feeds the `smoothedNetSpread`
telemetry only — sender and receiver steady-clock origins differ, so only
arrival deltas are usable for the DLL math.

### Rate loop

Each audio block:

1. Compute the fill error. With the DLL primed:
   `err = (predictedWriteNow - readPos) - target`, using the smooth
   predicted write position. Fallback path (DLL not yet primed):
   `err = fillNow - (prefill + period/2)`, where the `+period/2` term
   accounts for the half-amplitude of the natural fill oscillation
   between sender pumps. The prefill target itself is smoothed so
   `autoExtra` bumps/decays don't look like step changes to the loop.
2. Step a 3rd-order PI loop (two cascaded one-pole smoothers + a leaky
   integrator):

       z1 += w0 * (w1*err - z1);
       z2 += w0 * (z1 - z2);
       z3 += w2 * z2;
       rcorr = clamp (1 - (z2 + z3), 0.95 .. 1.05);

3. Compute the resampler ratio = `(outputSR / senderSR) * rcorr`.
4. Ask the resampler how many input frames it needs for one output
   block at that ratio, stage them out of the circular ring, and call
   `resampleProcessInterleaved`.
5. Sum the resampler output into the host buffer; advance the ring
   read pointer by exactly what the resampler consumed.

Loop bandwidth is `0.5 Hz` for the first ~4 s (fast lock), then
drops to `0.05 Hz` for smooth long-term tracking. If `|z3| > 0.05`
the loop resets and the peer re-primes — that should only happen
when the actual clock drift exceeds ±5%, which on real hardware
indicates a misconfigured device rather than normal drift.

The peer's output is muted while the loop locks in, so the lock-in
transient (rcorr pinned at its ±5 % clamp for a second or two) isn't
audible as a pitch slide. Unmute happens after ≥0.5 s once `|z2|` has
been below threshold for a run of consecutive blocks, with a ~10 s
safety cap.

Resampler defaults: 80 taps / 32 sub-filters with `SUBSAMPLE_INTERPOLATE`
for variable-ratio operation. Group delay = 40 input frames
≈ 0.83 ms at 48 kHz.

Set `MCFX_NET_DEBUG=1` in the environment to get per-second rate-loop
traces (rcorr, z2/z3, err, fill, prefill, autoExtra) on stderr.

## Threading

- NetThread (RT-scheduled, 20 ms period budget / 20 ms select timeout):
  drain-burst `recvfrom()` (cap 256 packets per iteration). Per packet:
  parse, dispatch by ptype. Control packets (INVITE/UNINVITE/ACK) update
  the accepted-senders set; DESC/DATA fall through to the audio path
  after the accepted-UID filter. Also runs the arrival-time DLL and the
  INVITE retry timer (re-INVITE every 1 s, Timeout after 5 s).
- Audio thread: lock-free read of the peer-list snapshot, sums each
  primed peer's frames into the output bus. Stale-peer eviction sweep
  also runs here under a `tryEnter` so we never block.
- Discovery thread: shared per-process `DiscoveryHub` advertiser +
  browser.

## Password

Both sides have an optional password field. The hash (SHA-256, or all
zeros if empty) travels in every INVITE; the receiving side compares
byte-for-byte and rejects mismatches with `IAR_BAD_PASSWORD` (surfaced
as `[bad password]` on the row and in the status banner). Empty on both
sides ≡ no auth. Casual access control on a shared LAN, NOT transport
encryption.

## What's *not* in v1

Same omissions as mcfx_send: no Opus, no NACK, no NAT traversal, no
encryption, no IPv6, no periodic STATUS packets.
See [mcfx_send/README.md](../mcfx_send/README.md) for the rationale.
