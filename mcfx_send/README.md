# mcfx_send

Multichannel UDP audio sender for the mcfx suite. Streams the input bus
to one or more `mcfx_receive` instances over IPv4 unicast.

## What it does

- Takes the plug-in's input bus (1–64 channels via VST3, up to 128 in AU /
  Standalone) and packetises it into UDP datagrams.
- Sends to **one or more** target receivers — picked from the
  Bonjour-discovered list, typed in by hand, or invited by the receiver.
- Supports PCM 16-bit, PCM 24-bit, and PCM 32-bit float on the wire.
- Adds `IP_TOS` / `SO_NET_SERVICE_TYPE` (DSCP-EF / Voice) so the audio
  packets travel in the high-priority class on Wi-Fi (WMM AC_VO) and any
  managed network that respects DSCP.
- Bumps `SO_SNDBUF` to 4 MB and uses 1400-byte packets so the kernel UDP
  queue doesn't overrun at high channel counts.
- Output bus is a passthrough of the input.

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

## Wire protocol

LAN-first, defined in
[common/net/mcfx_net_wire.h](../common/net/mcfx_net_wire.h):

- 16-byte header: `mcxa` magic, ptype, flags, sample format, channel
  count, sender UID (32-bit, random per process), monotonic sequence.
- DESC: sample rate, period, max packet size, sender steady-clock micros.
  Sent every 100 ms so a receiver that joins late can latch onto an
  in-progress stream.
- DATA: absolute frame count + N interleaved samples. Pack as many
  frames per packet as fit under the 1400-byte MTU, send N packets per
  audio period. Last packet of each period is flagged.
- INVITE / UNINVITE / INVITE_ACK: control packets for the handshake. The
  INVITE carries the inviter's listening UDP port and a SHA-256 hash of
  the inviter's password.

Little-endian on the wire (we run on x86/ARM only).

## Password

Both sides have an optional password field. The hash (SHA-256, or all
zeros if empty) travels in every INVITE; the receiving side compares
byte-for-byte and silently rejects mismatches. No popups — a refused
target shows up as `[bad password]` in the Targets list.

Empty on both sides ≡ no auth. The intent is casual access control on a
shared LAN, not transport encryption: anyone sniffing UDP can still read
the audio. Encryption is out of scope for v1.

## UI

- **Channels to send** — caps the wire channel count. Combo only lists
  values that fit the current input bus width.
- **Format** — PCM 16 / 24 / 32-bit float. Live; takes effect on the
  next packet without restarting the SendThread or socket.
- **Password** — gates inbound INVITEs and is sent in our outbound
  INVITEs. Empty = no auth. Persisted.
- **Local network** tab — Bonjour-discovered receivers, plus any
  active targets with no Bonjour match (Direct-IP path, or receiver
  that's gone from the network but the connection is still up).
  Status dot per row: gray = discovered/disconnected, yellow =
  inviting, green = connected, red = rejected (e.g. bad password),
  orange = no response. Double-click a row, or use the Connect /
  Disconnect buttons, to toggle. Per-target latency estimate shown
  inline when active.
- **Direct IP** tab — type a receiver's IP and listening UDP port to
  connect manually (no Bonjour required). The resulting connection
  shows up in the same unified list with a "(direct)" host label.
- **Network** line — bytes/sec, packets/sec, audio-thread overrun count.
- **Audio meters** — per-channel input level meters along the bottom.

## Latency

The Active targets list shows `lat~Xms` per active receiver where
known. The estimate is

```
sender_block_ms + receiver_jitter_ms + receiver_block_ms
```

The receiver's two values come back in the INVITE_ACK and are refreshed
on every (re-)INVITE. Network transit is excluded — that would need
NTP-grade synchronised clocks; on a LAN it's negligible vs. the buffer
+ block components.

## Threading

- Audio thread: interleaves the input bus into a lock-free SPSC FIFO
  (sized for four periods of headroom). RT-safe — no allocs, no locks.
- SendThread (RT-scheduled, 20 ms period budget): drains the FIFO,
  format-converts to the wire format, packetises, calls `sendto()` per
  active target. Also drives the INVITE retry timer for Pending targets
  (every 1 s, 5 s deadline before Timeout).
- NetThread (RT, 50 ms select timeout): drains inbound control packets
  (INVITE / UNINVITE / INVITE_ACK), updates the targets list, sends
  ACKs back to inviters that pass the password gate.
- Discovery thread: shared per-process `DiscoveryHub` listener +
  per-instance `Advertiser` for both browse (find receivers) and publish
  (be findable by receivers).

## What's *not* in v1

- No Opus codec. The sonolink Opus path was the source of every codec-
  side bug we hit; PCM-only first, Opus is deferred to v2.
- No NACK / packet-resend. If you need redundancy, pick a higher-rate
  format and accept the bandwidth.
- No NAT traversal, no encryption. LAN-first.
- No periodic STATUS packets — the sender's per-target latency display
  reflects the receiver's settings AT INVITE TIME. If the user changes
  the jitter floor on the receiver mid-session, the sender shows the
  stale value until the next handshake.
- No "auto-reconnect on plugin reload" for saved targets. The password
  is persisted across reloads; the targets list is intentionally not.
