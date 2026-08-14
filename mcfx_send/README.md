# mcfx_send

Multichannel UDP audio sender for the mcfx suite. Streams the input bus
to one or more `mcfx_receive` instances over IPv4 unicast.

## What it does

- Takes the plug-in's input bus (1–64 channels via VST3, up to 128 in AU /
  Standalone) and packetises it into UDP datagrams. The "Channels to send"
  combo caps the wire channel count, and offers explicit values up to 64
  plus "All".
- Sends to **one or more** target receivers — picked from the
  discovery list, typed in by hand, or invited by the receiver.
- Supports PCM 16-bit, PCM 24-bit, and PCM 32-bit float on the wire.
- Adds `IP_TOS` / `SO_NET_SERVICE_TYPE` (DSCP-EF / Voice) so the audio
  packets travel in the high-priority class on Wi-Fi (WMM AC_VO) and any
  managed network that respects DSCP.
- Bumps `SO_SNDBUF` to 4 MB and uses 1400-byte packets so the kernel UDP
  queue doesn't overrun at high channel counts.
- Output bus is a passthrough of the input.
- Survives anticipative / offline rendering: the SendThread paces itself
  against wallclock, and `pushAudioBlock` detects sustained faster-than-
  realtime calls (REAPER's anticipative FX) and drops out of the FIFO
  entirely until the host returns to realtime.

## Connection model

Symmetric. Either side can initiate. Every connection — manual entry,
discovery click, or inbound INVITE — goes through the same INVITE /
INVITE_ACK handshake before audio flows. This is what makes the password
gate effective.

- **Sender → Receiver** (you click a discovered receiver): the sender
  sends an INVITE to that receiver. The receiver password-checks, ACKs,
  and starts accepting the stream.
- **Receiver → Sender** (a receiver clicks our advertised entry): the
  receiver sends an INVITE to us. We password-check, ACK, add the
  receiver to the targets list, and start streaming.
- **UNINVITE**: clicking Disconnect on either side sends a courtesy
  UNINVITE to the peer; the peer drops the entry from its list.

The sender keeps a list of up to 16 active targets; sendto() is called
per-target per packet. Bandwidth scales linearly with the target count.
A 17th INVITE is refused with `IAR_MAX_TARGETS`.

## Wire protocol

LAN-first, defined in
[common/net/mcfx_net_wire.h](../common/net/mcfx_net_wire.h):

- 16-byte header: `mcxb` magic, ptype, flags, sample format, channel
  count, sender UID (32-bit, random per process), monotonic sequence.
  (`mcxa` was the v1 layout and is wire-incompatible — the magic check
  rejects it.)
- DESC: sample rate, period, max packet size, sender steady-clock micros.
  Sent every 100 ms so a receiver that joins late can latch onto an
  in-progress stream.
- DATA: sender steady-clock micros at `sendto()` time (`tx_us`), absolute
  frame count, frame count in this packet, then N interleaved samples.
  Pack as many frames per packet as fit under the 1400-byte MTU, send N
  packets per audio period. Last packet of each period is flagged — the
  receiver's arrival-time DLL updates once per flagged packet.
- INVITE / UNINVITE / INVITE_ACK: control packets for the handshake. The
  INVITE carries the inviter's listening UDP port and a SHA-256 hash of
  the inviter's password.

Little-endian on the wire (we run on x86/ARM only).

## Discovery

Not mDNS/Bonjour despite the naming in places — it's JUCE's
`NetworkServiceDiscovery`, i.e. periodic UDP broadcast on the local
subnet ([common/net/mcfx_net_discovery.h](../common/net/mcfx_net_discovery.h)):

- Senders advertise under `mcfx_send.v1` on broadcast port **35517**;
  receivers advertise under `mcfx_receive.v1` on **35518**. Two ports
  because macOS delivers a broadcast to only one listener per port, so
  a sender and a receiver in the same DAW process would otherwise fight
  over it.
- TXT fields: `uid`, `track`, `host`, `user`, `project`, `ch`, `fmt`, and
  `wuid` — the 32-bit wire UID, which is what the editors use to pair a
  discovered row with a targets-list entry (host:port drifts across
  re-binds and multi-homed interfaces; the UID doesn't).
- A per-process `DiscoveryHub` shares one listener socket across all
  plug-in instances in the DAW. A browser also publishes an empty
  heartbeat under the *other* side's UID so macOS opens the
  Local Network privacy gate for inbound broadcasts.

Anything the broadcast can't reach (different subnet, mDNS/broadcast
blocked) is still reachable through the Direct IP tab.

## Password

Both sides have an optional password field. The hash (SHA-256, or all
zeros if empty) travels in every INVITE; the receiving side compares
byte-for-byte and rejects mismatches with `IAR_BAD_PASSWORD`. No popups —
a refused target shows up as `[bad password]` in the list and the entry
stays visible for 30 s so the failure is noticeable.

Empty on both sides ≡ no auth. The intent is casual access control on a
shared LAN, not transport encryption: anyone sniffing UDP can still read
the audio. Encryption is out of scope for v1.

## UI

- **Channels to send** — caps the wire channel count. Editable combo:
  pick a value, or type one (clamped to the bus width); "All" sends
  whatever the bus exposes.
- **Format** — PCM 16 / 24 / 32-bit float. Live; takes effect on the
  next packet without restarting the SendThread or socket.
- **Password** — gates inbound INVITEs and is sent in our outbound
  INVITEs. Empty = no auth. Persisted.
- **Auto-reconnect** — toggle, persisted. See below.
- **Listening on UDP port N** — our own control port, plus this machine's
  IPv4 addresses, so the number can be read off the screen and handed to
  a peer who has to type it into their Direct IP tab.
- **Status line** — "N active targets  uid=…", or the auto-reconnect
  countdown while it's running.
- **Local network** tab — discovered receivers, plus any active targets
  with no discovery match (Direct-IP path, or a receiver that's gone from
  the network but the connection is still up). A filter box matches on
  host / project / track / IP. Status dot per row: gray = discovered/
  disconnected, yellow = inviting, green = connected, red = rejected
  (e.g. bad password), orange = no response. Double-click a row, or use
  the Connect / Disconnect buttons, to toggle.
- **Direct IP** tab — type a receiver's IP and listening UDP port to
  connect manually (no discovery required). The resulting connection
  shows up in the same unified list with a "(direct)" host label.
- **Network** line — bytes/sec, packets/sec, audio-thread overrun count.
- **Audio meters** — per-channel input level meters along the bottom.

## Auto-reconnect

Targets that were Active or Pending at save time are written into the
plug-in state, together with the discovery identity (host / project /
track) captured when the user clicked Connect. On reload — unless the
Auto-reconnect toggle is off — those peers are *armed* and a 1 Hz timer
re-issues the connect for up to 60 s:

- If a discovered service matches the saved identity at a different
  address, the armed entry swaps to the current IP:port before inviting.
- An armed entry is dropped as soon as the peer is Active — matched by
  host:port, by wire UID, by discovery identity, or (fallback, for when
  the *other* side's auto-reconnect got there first) by any Active target
  at the same IP.
- On expiry the stale entry is uninvited so the list isn't left with a
  dead "Inviting" row.

Any user click on Connect / Disconnect cancels the whole armed set — the
user is now driving.

## Latency

The receiver reports its jitter-buffer floor and audio period back in the
INVITE_ACK, refreshed on every (re-)INVITE, and the sender keeps those
per target. They are currently *not* rendered in the sender UI — the
per-peer `lat ~Xms` readout lives on the receiver side, where the
adaptive part of the buffer and the resampler group delay are also known.

Network transit is excluded from that estimate — it would need NTP-grade
synchronised clocks; on a LAN it's negligible vs. the buffer + block
components.

## Threading

- Audio thread: interleaves the input bus into a lock-free SPSC FIFO
  (sized for four periods of headroom). RT-safe — no allocs, no locks.
- SendThread (RT-scheduled): drains the FIFO, format-converts to the
  wire format, packetises, calls `sendto()` per active target. Paced
  against wallclock so an anticipative/offline host can't flood the
  receiver. Every 100 ms it also emits the descriptor packet and ticks
  the INVITE retry timer (re-INVITE every 1 s, Timeout after 5 s).
  Sleeps until the next period boundary, at most 20 ms.
- NetThread (RT, 50 ms select timeout, 32-packet drain burst): handles
  inbound control packets (INVITE / UNINVITE / INVITE_ACK), updates the
  targets list, sends ACKs back to inviters that pass the password gate.
- Discovery thread: shared per-process `DiscoveryHub` listener +
  per-instance advertiser for both browse (find receivers) and publish
  (be findable by receivers).

## What's *not* in v1

- No Opus codec. The sonolink Opus path was the source of every codec-
  side bug we hit; PCM-only first, Opus is deferred to v2.
- No NACK / packet-resend. If you need redundancy, pick a higher-rate
  format and accept the bandwidth.
- No NAT traversal, no encryption, no IPv6. LAN-first.
- No periodic STATUS packets — the receiver-reported numbers a sender
  holds per target reflect the receiver's settings AT INVITE TIME and go
  stale until the next handshake.
