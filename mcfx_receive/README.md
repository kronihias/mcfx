# mcfx_receive

Multichannel UDP audio receiver for the mcfx suite. Listens on a UDP
port, decodes the mcfx_send wire protocol, and mixes one or more
incoming streams into the output bus.

## What it does

- Binds an ephemeral UDP port automatically; reports the bound port in
  the UI.
- Per packet: parses the mcfx wire header, looks up the sender by UID,
  routes the audio frames into that sender's per-peer ring buffer.
- Supports PCM 16-bit, PCM 24-bit, and PCM 32-bit float on the wire.
  Decodes int16/int24 to float at NetThread time; the audio thread
  always reads float frames.
- On audio callback: pulls `blockSize` frames from each primed peer
  through a per-peer variable-ratio sinc resampler and sums into the
  output bus. The resampler ratio is nudged each block by a control
  loop on the ring fill so the receiver stays phase-locked to the
  sender even when the two hardware audio clocks don't agree exactly.
- Adaptive jitter buffer: user-set floor in ms + per-peer auto extra
  that grows on packet loss / underrun and decays slowly during clean
  periods. Bounded at 500 ms total per peer.
- Stale peer eviction: a peer that goes silent for >15 s is dropped.
- Bumps `SO_RCVBUF` to 4 MB and applies QoS markings (matches mcfx_send).
- Publishes its bound port via Bonjour as `mcfx_receive.v1` so senders
  on the same LAN see it in their browse list.
- Browses for senders under `mcfx_send.v1` so the receiver UI can show
  the sender's-eye view too.

## Connection model

Symmetric. Audio only flows for senders we've explicitly accepted via
the INVITE handshake; inbound DATA from any other UID is dropped with a
DBG log. Three ways to get a sender into the accepted set:

1. **Click a discovered sender** (Senders list) — the receiver sends an
   INVITE to that sender. The sender password-checks, ACKs, adds us to
   its targets list, and starts streaming. Our INVITE_ACK callback
   marks the entry Active so DATA packets pass the filter.
2. **Inbound INVITE** (sender clicks our advertised entry, or types our
   address manually) — the sender sends us an INVITE; we password-check,
   ACK, and add the sender to our accepted set as Active immediately.
3. **Click Disconnect** in the Active senders list — sends UNINVITE.

The accepted set lives only in memory; on plugin reload it's empty and
the user re-clicks. Password is persisted.

## UI

- **Listening on UDP port N** — read-only display of the ephemeral port
  the receiver bound at startup. Sent in Bonjour TXT so senders find us.
- **Jitter floor (ms)** — minimum buffer the receiver waits for before
  starting playback for a peer. Adaptive auto-extra adds on top per-peer
  in response to actual loss; the per-peer status line shows the
  current effective value.
- **Password** — gates inbound INVITEs and is sent in our outbound
  INVITEs. Empty = no auth. Persisted.
- **Local network** tab — Bonjour-discovered senders, plus any active
  senders with no Bonjour match (Direct-IP path or sender that's gone
  from the network but the connection is still up). Status dot per row:
  gray = discovered/disconnected, yellow = inviting, green = connected,
  red = rejected (e.g. bad password), orange = no response. Double-
  click a row, or use the Connect/Disconnect buttons, to toggle.
- **Direct IP** tab — type a sender's IP and listening UDP port to
  connect manually (no Bonjour required). The resulting connection
  shows up in the same unified list with a "(direct)" host label.
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

Each peer carries an `autoExtraMs` value added to the user-set floor:

- On `gap` (NetThread sees the data sequence skip): bump `autoExtraMs`
  by `max(20, autoExtraMs/2)`, capped at 200. Sets last-loss timestamp.
- On `underrun` (audio thread couldn't fill the block): same bump, plus
  drop the peer back into "priming" mode.
- Each audio block, if no loss in the last 5 s, decay `autoExtraMs` by
  1 ms (rate-limited to one tick every ~5 ms so the decay rate is
  independent of host block size).

Fast to react, slow to give back latency.

## Adaptive sample-rate conversion

Sender and receiver run on independent audio clocks. Even when the
two devices report the same nominal rate (e.g. both 48000 Hz), the
physical crystals drift a few ppm — enough that, over minutes, a
fixed-rate read would either starve or overflow the ring. To stay
phase-locked we run each peer's audio through a per-peer variable-
ratio sinc resampler (vendored
[dbry/audio-resampler](https://github.com/dbry/audio-resampler), BSD-3,
under [common/third_party/audio-resampler](../common/third_party/audio-resampler)).

Each audio block:

1. Compute the ring fill error: `err = fillNow - (prefill + period/2)`.
   The `+period/2` term accounts for the half-amplitude of the natural
   fill oscillation between sender pumps; without it the integrator
   would have a persistent positive bias and trip the anti-windup
   ceiling every few seconds.
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

Resampler defaults: 80 taps / 32 sub-filters with `SUBSAMPLE_INTERPOLATE`
for variable-ratio operation. Group delay = 40 input frames
≈ 0.83 ms at 48 kHz.

## Threading

- NetThread (RT-scheduled, 20 ms period budget): `select()` then
  drain-burst `recvfrom()` (cap 256 packets per iteration). Per packet:
  parse, dispatch by ptype. Control packets (INVITE/UNINVITE/ACK) update
  the accepted-senders set; DESC/DATA fall through to the audio path
  after the accepted-UID filter.
- Audio thread: lock-free read of the peer-list snapshot, sums each
  primed peer's frames into the output bus. Stale-peer eviction sweep
  also runs here under a `tryEnter` so we never block.
- Discovery thread: shared per-process `DiscoveryHub` advertiser +
  browser.

## Bonjour

Receivers publish under `mcfx_receive.v1` on UDP `35517` with TXT
fields: `uid`, `track`, `host`, `user`, `ch`, `fmt`. Senders publish
under `mcfx_send.v1`. Each side browses the OTHER side's UID and shows
the result in its own UI.

## Password

Both sides have an optional password field. The hash (SHA-256, or all
zeros if empty) travels in every INVITE; the receiving side compares
byte-for-byte and silently rejects mismatches. Empty on both sides ≡
no auth. Casual access control on a shared LAN, NOT transport
encryption.

## What's *not* in v1

Same omissions as mcfx_send: no Opus, no NACK, no NAT traversal, no
encryption,  no periodic STATUS packets.
See [mcfx_send/README.md](../mcfx_send/README.md) for the rationale.
