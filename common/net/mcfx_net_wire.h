/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================

 mcfx_net wire format — UDP audio streaming protocol used by mcfx_send /
 mcfx_receive. LAN-first:
   * Little-endian on the wire (we run on x86/ARM only — no need to byte-
     swap every packet for big-endian sympathy).
   * 32-bit sender UID — pairs with the Bonjour TXT 'uid' field so a
     receiver can recognise a sender that restarted on a new UDP port.
   * No frame-time field in the data packet (derivable from `count` plus
     the descriptor packet's clock).

 ==============================================================================
*/

#pragma once

#include <cstdint>
#include <cstring>

namespace mcfx { namespace net {

// 4-byte magic. Bumps for protocol breaks; receivers MUST drop packets that
// don't match. "mcxb" = mcfx audio v2 — DataPayload now carries a sender
// steady-clock timestamp ahead of count/nfram so the receiver can drive a
// per-arrival DLL for clock-drift compensation. The previous "mcxa" layout
// is wire-incompatible and is rejected by the magic check.
inline constexpr char kMagic[4] = { 'm', 'c', 'x', 'b' };

// Common header types.
enum PacketType : std::uint8_t
{
    PT_UNKNOWN    = 0,
    PT_DESC       = 1,   // ADESC  — stream descriptor (sample rate, period, ...)
    PT_DATA       = 2,   // ADATA  — audio frames
    PT_INVITE     = 3,   // peer → us: please add me to your target list
    PT_UNINVITE   = 4,   // peer → us: stop sending to me
    PT_INVITE_ACK = 5,   // us → peer: invite outcome
};

enum InviteAckReason : std::uint8_t
{
    IAR_OK            = 0,
    IAR_BAD_PASSWORD  = 1,
    IAR_MAX_TARGETS   = 2,
    IAR_INTERNAL      = 3,
};

// Sample format codes — must round-trip on the wire, so values are stable.
enum SampleFormat : std::uint8_t
{
    SF_UNKNOWN = 0,
    SF_INT16   = 1,
    SF_INT24   = 2,   // 3 bytes per sample, little-endian (mid is 24-bit)
    SF_FLOAT32 = 3,
};

inline constexpr int sampleSizeBytes (SampleFormat sf) noexcept
{
    return sf == SF_INT16   ? 2
         : sf == SF_INT24   ? 3
         : sf == SF_FLOAT32 ? 4
         : 0;
}

// Flags bitmask in the header. Designed so that 0 is a sane default.
enum HeaderFlags : std::uint8_t
{
    HF_LAST_OF_PERIOD = 0x01, // this is the last data packet for this period
    HF_MULTICAST_HINT = 0x02, // sender is broadcasting on a multicast group
};

// 16-byte common header. Identical layout for ADESC and ADATA — payload
// follows immediately. All multi-byte integers are little-endian.
//
// Total size deliberately a multiple of 8 so the audio payload that
// follows is naturally aligned (matters for some int24/float32 SIMD work).
#pragma pack(push, 1)
struct CommonHeader
{
    char          magic[4];   // "mcxa"
    std::uint8_t  ptype;      // PacketType
    std::uint8_t  flags;      // HeaderFlags
    std::uint8_t  sfmt;       // SampleFormat
    std::uint8_t  nchan;      // 1..64
    std::uint32_t sender;     // UID — random per process; unique per logical sender
    std::uint32_t seq;        // monotonic per (sender, stream)
};
static_assert (sizeof (CommonHeader) == 16, "CommonHeader must be 16 bytes on the wire");

// Stream descriptor (ADESC). Sent on stream start and periodically (~10 Hz)
// so a freshly-attached receiver can latch onto an in-progress stream.
// Steady-clock micros let the receiver implement an arrival-time DLL in
// a follow-up version — v1 ignores them but the field stays so we don't
// break the wire format later.
struct DescPayload
{
    std::uint32_t fsamp;      // sender sample rate (e.g. 48000)
    std::uint32_t period;     // sender audio period in frames (DAW block)
    std::uint32_t pktsz;      // max payload bytes per ADATA packet
    std::uint64_t steady_us;  // sender's steady-clock at the moment this was sent
};
static_assert (sizeof (DescPayload) == 20, "DescPayload size mismatch");

// Audio data (ADATA). count is the absolute starting sample index in the
// sender's stream; it lets the receiver detect packet loss precisely (gap)
// and reordering (count goes backwards). nfram is how many sample frames
// follow in this packet. tx_us is the sender's steady_clock micros sampled
// at sendto() time and feeds the receiver's per-arrival DLL — it drives
// clock-drift compensation for cross-machine streams. (Steady-clock
// origins differ per machine, so the absolute value is meaningless across
// hosts; only deltas/per-stream interpretation matter.)
struct DataPayload
{
    std::uint64_t tx_us;       // sender steady_clock micros at sendto()
    std::uint32_t count;       // absolute sample index of first frame in this packet
    std::uint16_t nfram;       // frames in this packet
    std::uint16_t reserved;    // 0
    // ... interleaved samples follow ...
};
static_assert (sizeof (DataPayload) == 16, "DataPayload size mismatch");

// PT_INVITE — sent from a peer that wants to be added to the recipient's
// target list. Recipient is the sender plug-in; it password-checks and
// adds the peer to its targets if accepted, then replies with PT_INVITE_ACK.
// Inviter's listening port is in `listen_port`; the inviter's IP comes
// from the UDP source address. The CommonHeader's `sender` field is the
// inviter's UID.
struct InvitePayload
{
    std::uint32_t listen_port;          // inviter's listening UDP port
    std::uint8_t  pw_hash[32];          // SHA-256 of the inviter's password (zeros if empty)
    std::uint8_t  reserved[12];         // padded to 48 bytes total
};
static_assert (sizeof (InvitePayload) == 48, "InvitePayload size mismatch");

// PT_UNINVITE — counterpart of INVITE. The recipient drops the inviter
// from its target list. listen_port matches the prior INVITE so the
// recipient identifies the right entry.
struct UninvitePayload
{
    std::uint32_t listen_port;
};
static_assert (sizeof (UninvitePayload) == 4, "UninvitePayload size mismatch");

// PT_INVITE_ACK — sent in reply to INVITE. accepted=1 + reason=IAR_OK
// means audio will start flowing on the configured stream. The two
// receiver-side numbers piggyback on the ACK so the sender can render
// a meaningful per-target latency number without a separate STATUS
// packet — they're refreshed on every (re-)INVITE.
struct InviteAckPayload
{
    std::uint8_t  accepted;             // 1 = accepted, 0 = rejected
    std::uint8_t  reason;               // InviteAckReason
    std::uint16_t reserved;
    std::uint32_t receiver_jitter_ms;   // for sender-side latency display
    std::uint32_t receiver_period;      // ditto, in frames
};
static_assert (sizeof (InviteAckPayload) == 12, "InviteAckPayload size mismatch");
#pragma pack(pop)

inline constexpr int kHeaderBytes        = sizeof (CommonHeader);
inline constexpr int kDescTotalBytes     = sizeof (CommonHeader) + sizeof (DescPayload);
inline constexpr int kDataHeaderBytes    = sizeof (CommonHeader) + sizeof (DataPayload);
inline constexpr int kInviteTotalBytes   = sizeof (CommonHeader) + sizeof (InvitePayload);
inline constexpr int kUninviteTotalBytes = sizeof (CommonHeader) + sizeof (UninvitePayload);
inline constexpr int kInviteAckTotalBytes= sizeof (CommonHeader) + sizeof (InviteAckPayload);

// Conservative MTU default — fits under standard Ethernet 1500 with room
// for IP+UDP (28 B) and typical VPN/IPsec overhead. Same value SonoLink
// settled on after the high-channel-count packet-rate firefighting.
inline constexpr int kDefaultMaxPacketBytes = 1400;

// Sanity check the leading bytes of a UDP datagram. Not a parse — just
// fast-rejects garbage before allocation. Returns true if the packet
// might plausibly be one of ours.
inline bool looksLikeMcfxPacket (const void* data, int nbytes) noexcept
{
    if (nbytes < kHeaderBytes) return false;
    return std::memcmp (data, kMagic, sizeof (kMagic)) == 0;
}

// How many ADATA packets does it take to ship `period` frames of `nchan`
// channels at `sfmt`, given a max-payload-bytes budget? Pack as many
// frames per packet as fit, send N packets per period.
inline int packetsPerPeriod (int period, int nchan, SampleFormat sfmt,
                             int maxPacketBytes = kDefaultMaxPacketBytes) noexcept
{
    if (period <= 0 || nchan <= 0) return 0;
    const int sampSize = sampleSizeBytes (sfmt);
    if (sampSize <= 0) return 0;
    const int payloadBudget = maxPacketBytes - kDataHeaderBytes;
    if (payloadBudget <= 0) return 0;
    const int framesPerPacket = payloadBudget / (sampSize * nchan);
    if (framesPerPacket <= 0)
        return period; // pathological: 1 frame won't fit; fall back to 1-frame packets
    return (period + framesPerPacket - 1) / framesPerPacket;
}

// Inverse of the above — how many frames go into one packet given the same
// inputs. Useful for the sender's packetizer loop.
inline int framesPerPacket (int nchan, SampleFormat sfmt,
                            int maxPacketBytes = kDefaultMaxPacketBytes) noexcept
{
    const int sampSize = sampleSizeBytes (sfmt);
    if (sampSize <= 0 || nchan <= 0) return 0;
    const int payloadBudget = maxPacketBytes - kDataHeaderBytes;
    return payloadBudget / (sampSize * nchan);
}

}} // namespace mcfx::net
