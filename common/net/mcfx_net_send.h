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

 mcfx_net SendStream — owns the UDP socket and the sender-side data path.

 Threading:
   * Audio thread   — pushAudioBlock() interleaves & writes float frames into
                      a lock-free SPSC FIFO. RT-safe (no allocs, no locks).
   * SendThread     — drains the FIFO, format-converts to wire sample format
                      (int16/int24/float32), packetises per the wire protocol,
                      sendto()s. Periodically emits a descriptor packet so a
                      late-joining receiver can latch on. Also drives the
                      INVITE retry timer for pending targets.
   * NetThread      — recvfrom() on the same socket. Parses control packets
                      (INVITE/UNINVITE/INVITE_ACK), updates target state,
                      sends INVITE_ACK back to inviters that pass the
                      password gate.
   * Message thread — addTarget() / removeTarget() / setPassword(), etc.

 Multi-target: a SendStream can fan out to N receivers simultaneously.
 Each Target is added either explicitly (user clicks a discovered receiver
 → addTarget triggers an INVITE handshake) or implicitly (a receiver
 sends us an INVITE, we password-check + accept). Either way the same
 audio packet stream gets sendto()'d to every Active target per period.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "mcfx_net_wire.h"

#include "mcfx_net_socket.h"

#include <atomic>
#include <memory>
#include <vector>

namespace mcfx { namespace net {

class SendStream
{
public:
    enum class TargetState : std::uint8_t
    {
        Pending,    // INVITE sent, waiting for ACK
        Active,     // ACK received, audio flowing
        Rejected,   // ACK received with reason != OK (kept briefly for UI)
        Timeout,    // No ACK within retry window (kept briefly for UI)
    };

    struct Target
    {
        sockaddr_in   addr {};
        juce::String  host;             // dotted IP literal
        int           port = 0;
        uint32_t      receiverUid = 0;  // 0 until known (from INVITE_ACK or DESC)
        juce::String  label;            // display string for the UI
        TargetState   state = TargetState::Pending;
        InviteAckReason rejectReason = IAR_OK;
        juce::Time    addedAt;
        juce::Time    lastInviteAt;
        // Receiver-reported numbers, populated on INVITE_ACK. Used by the
        // sender's UI to display per-target latency without needing a
        // periodic STATUS round-trip.
        int receiverJitterMs = 0;
        int receiverPeriod   = 0;
        int receiverSampleRate = 0;
    };

    SendStream();
    ~SendStream();

    // Stream format. Allocates the audio FIFO and starts SendThread+NetThread.
    void prepareToPlay (double sampleRate, int blockSize,
                        int numChannels, SampleFormat fmt);

    void releaseResources();

    void setRingChannels (int numChannels);

    void setWireFormat (SampleFormat fmt) noexcept
    {
        wireFormat.store (fmt, std::memory_order_release);
    }
    SampleFormat getWireFormat() const noexcept
    {
        return wireFormat.load (std::memory_order_acquire);
    }

    // Audio thread: push numSamples frames of nchan channels.
    bool pushAudioBlock (const float* const* channelData,
                         int numChannels, int numSamples);

    // ----- Target management -------------------------------------------------

    // Add a target by host:port. Triggers an INVITE handshake; the target
    // becomes Active only after an INVITE_ACK with reason=OK arrives. If
    // the same host:port is already in the list, this re-INVITE just
    // refreshes the existing entry. Returns false on bad input.
    bool addTarget (const juce::String& host, int port);

    // Remove a target. Sends a courtesy UNINVITE and drops the entry.
    void removeTarget (const juce::String& host, int port);

    // Drop every target. Sends UNINVITE to each.
    void clearTargets();

    // Snapshot for the UI. Cheap.
    std::vector<Target> getTargets() const;

    int getActiveTargetCount() const noexcept;

    // Replace mode for the manual-entry "Apply" button: clearTargets +
    // addTarget. Empty host clears.
    bool setSingleTarget (const juce::String& host, int port);

    // Password used in outbound INVITEs and to gate inbound INVITEs.
    // Empty == no auth (matches all-zeros hash).
    void setPassword (const juce::String& pw);
    juce::String getPassword() const;

    // ----- Telemetry ---------------------------------------------------------

    uint64_t getSentPackets() const noexcept { return sentPackets.load (std::memory_order_relaxed); }
    uint64_t getSentBytes()   const noexcept { return sentBytes.load   (std::memory_order_relaxed); }
    uint64_t getOverruns()    const noexcept { return audioOverruns.load (std::memory_order_relaxed); }

    uint32_t getSenderUid() const noexcept { return senderUid; }

    // The UDP port we're listening on for inbound control packets. Lets
    // Bonjour advertise the right address so receivers can invite us.
    int getListenPort() const noexcept { return listenPort; }

private:
    class SendThread;
    class NetThread;

    void sendDescriptorTo (const sockaddr_in& addr) noexcept;
    void sendDescriptorToAllActive() noexcept;
    void sendPeriodFromFifoToAllActive() noexcept;
    void sendOneInvite (Target& t) noexcept;
    void sendOneUninvite (const Target& t) noexcept;
    void sendInviteAck (const sockaddr_in& to, const InviteAckPayload& body) noexcept;

    // SendThread side: walks the targets list and re-INVITEs Pending
    // targets every 1 s up to a 5 s deadline. Promotes Rejected/Timeout
    // entries to "drop after a few seconds of UI display" via a stale
    // sweep.
    void tickInviteRetries() noexcept;

    // NetThread side: dispatched per inbound packet by ptype.
    void handleInvitePacket (const sockaddr_in& from, const CommonHeader& hdr,
                             const InvitePayload& body) noexcept;
    void handleUninvitePacket (const sockaddr_in& from, const CommonHeader& hdr,
                               const UninvitePayload& body) noexcept;
    void handleInviteAckPacket (const sockaddr_in& from, const CommonHeader& hdr,
                                const InviteAckPayload& body) noexcept;

    static void hashPassword (const juce::String& pw, std::uint8_t out[32]) noexcept;
    bool passwordMatches (const std::uint8_t inviterHash[32]) const noexcept;

    std::unique_ptr<juce::DatagramSocket> socket;
    bool socketBound = false;
    int  listenPort  = 0;

    juce::CriticalSection targetsLock;
    std::vector<Target>   targets;

    juce::CriticalSection passwordLock;
    juce::String          password;

    juce::CriticalSection reconfigLock;

    int  ringBlockSize   = 0;
    int  ringNumChannels = 0;
    SampleFormat ringFormat = SF_FLOAT32;
    std::atomic<SampleFormat> wireFormat { SF_FLOAT32 };
    int  ringSampleRate = 48000;

    std::vector<float>      ringBuffer;
    std::unique_ptr<juce::AbstractFifo> fifo;

    std::vector<float>      drainBuffer;
    std::vector<uint8_t>    netBuffer;

    uint32_t              senderUid = 0;
    std::atomic<uint32_t> nextSeq { 0 };
    std::atomic<uint32_t> absoluteFrame { 0 };

    std::atomic<uint64_t> sentPackets    { 0 };
    std::atomic<uint64_t> sentBytes      { 0 };
    std::atomic<uint64_t> audioOverruns  { 0 };

    std::atomic<bool> running { false };
    juce::WaitableEvent wake;

    std::unique_ptr<SendThread> sendThread;
    std::unique_ptr<NetThread>  netThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendStream)
};

}} // namespace mcfx::net
