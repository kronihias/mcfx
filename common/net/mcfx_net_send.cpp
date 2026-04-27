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
*/

#include "mcfx_net_send.h"
#include "mcfx_net_socket.h"
#include "mcfx_net_rt.h"

#include <chrono>
#include <random>
#include <cstring>

namespace mcfx { namespace net {

// Re-INVITE every 1 s while a target is Pending. Drop to Timeout after 5 s
// without an ACK. After Rejected/Timeout, leave the entry in the list for
// 5 s so the UI can render the failure, then sweep.
constexpr int    kInviteRetryMs       = 1000;
constexpr int    kInviteDeadlineMs    = 5000;
// Keep Rejected/Timeout entries visible for 30 s so the user sees
// "[bad password]" / "[no response]" feedback instead of the entry
// vanishing the moment they look at the screen.
constexpr int    kRejectKeepMs        = 30000;
constexpr int    kMaxTargets          = 16;

// ---------------------------------------------------------------------------

class SendStream::SendThread : public juce::Thread
{
public:
    explicit SendThread (SendStream& o) : juce::Thread ("mcfx_send"), owner (o) {}

    void run() override
    {
        setCurrentThreadRealtime (20.0, 3.0, 10.0);

        owner.sendDescriptorToAllActive();
        auto lastDescTime = juce::Time::getMillisecondCounterHiRes();

        while (! threadShouldExit())
        {
            while (owner.fifo != nullptr
                   && owner.fifo->getNumReady() >= owner.ringBlockSize * owner.ringNumChannels)
            {
                owner.sendPeriodFromFifoToAllActive();
            }

            const double now = juce::Time::getMillisecondCounterHiRes();
            if (now - lastDescTime > 100.0)
            {
                owner.sendDescriptorToAllActive();
                owner.tickInviteRetries();
                lastDescTime = now;
            }

            owner.wake.wait (20);
        }
    }

    SendStream& owner;
};

// NetThread mirrors the receiver's recv pattern. It exists ONLY to handle
// inbound control packets (INVITE/UNINVITE/INVITE_ACK); the audio data
// path is one-way (us → targets), so all the heavy lifting still happens
// on SendThread. Drain-burst pattern is overkill for the trickle of
// control packets but keeps the code shape uniform with RecvStream and
// is harmless.
class SendStream::NetThread : public juce::Thread
{
public:
    explicit NetThread (SendStream& o) : juce::Thread ("mcfx_send_net"), owner (o) {}

    void run() override
    {
        setCurrentThreadRealtime (20.0, 1.0, 10.0);
        while (! threadShouldExit())
        {
            if (! owner.socketBound || ! owner.socket)
            {
                juce::Thread::sleep (5);
                continue;
            }
            if (! owner.socket->waitUntilReady (true, 50)) continue;

            uint8_t buf[2048];
            juce::String fromHost;
            int fromPort = 0;
            constexpr int kMaxBurst = 32;
            for (int i = 0; i < kMaxBurst; ++i)
            {
                const auto nbytes = owner.socket->read (buf, sizeof (buf), false,
                                                         fromHost, fromPort);
                if (nbytes <= 0) break;
                if (! looksLikeMcfxPacket (buf, (int) nbytes)) continue;

                CommonHeader hdr {};
                std::memcpy (&hdr, buf, sizeof (hdr));

                sockaddr_in from {};
                from.sin_family = AF_INET;
                from.sin_port   = htons (static_cast<uint16_t> (fromPort));
                ::inet_pton (AF_INET, fromHost.toRawUTF8(), &from.sin_addr);

                if (hdr.ptype == PT_INVITE
                    && (int) nbytes >= kInviteTotalBytes)
                {
                    InvitePayload body {};
                    std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
                    owner.handleInvitePacket (from, hdr, body);
                }
                else if (hdr.ptype == PT_UNINVITE
                         && (int) nbytes >= kUninviteTotalBytes)
                {
                    UninvitePayload body {};
                    std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
                    owner.handleUninvitePacket (from, hdr, body);
                }
                else if (hdr.ptype == PT_INVITE_ACK
                         && (int) nbytes >= kInviteAckTotalBytes)
                {
                    InviteAckPayload body {};
                    std::memcpy (&body, buf + sizeof (CommonHeader), sizeof (body));
                    owner.handleInviteAckPacket (from, hdr, body);
                }
                // Anything else (DESC/DATA from a stray sender, garbage):
                // silently drop. We only care about control packets here.
            }
        }
    }

    SendStream& owner;
};

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

SendStream::SendStream()
{
    std::random_device rd;
    std::mt19937 gen (rd());
    std::uniform_int_distribution<uint32_t> dist;
    senderUid = dist (gen);
    if (senderUid == 0) senderUid = 1;
}

SendStream::~SendStream()
{
    releaseResources();
}

void SendStream::prepareToPlay (double sampleRate, int blockSize,
                                int numChannels, SampleFormat fmt)
{
    const juce::ScopedLock sl (reconfigLock);

    releaseResources();

    if (blockSize <= 0 || numChannels <= 0) return;

    ringSampleRate  = static_cast<int> (sampleRate);
    ringBlockSize   = blockSize;
    ringNumChannels = numChannels;
    ringFormat      = fmt;
    wireFormat.store (fmt, std::memory_order_release);

    const int capacity = 4 * blockSize * numChannels;
    ringBuffer.assign (static_cast<size_t> (capacity), 0.0f);
    fifo = std::make_unique<juce::AbstractFifo> (capacity);

    drainBuffer.assign (static_cast<size_t> (blockSize * numChannels), 0.0f);

    netBuffer.assign (static_cast<size_t> (kDataHeaderBytes
                                           + blockSize * numChannels * sampleSizeBytes (SF_FLOAT32)),
                      0);

    if (! socketBound)
    {
        socket = std::make_unique<juce::DatagramSocket>();
        if (! socket->bindToPort (0))
        {
            DBG ("mcfx_send: socket bindToPort(0) failed");
            socket.reset();
            return;
        }
        socketBound = true;
        listenPort = socket->getBoundPort();

        const auto rawHandle = socket->getRawSocketHandle();
        tuneKernelUdpBuffers (rawHandle, "mcfx_send");
        applyAudioQos (rawHandle, "mcfx_send");
    }

    nextSeq.store (0, std::memory_order_relaxed);
    absoluteFrame.store (0, std::memory_order_relaxed);
    sentPackets.store (0, std::memory_order_relaxed);
    sentBytes.store (0, std::memory_order_relaxed);
    audioOverruns.store (0, std::memory_order_relaxed);

    running.store (true, std::memory_order_release);
    sendThread = std::make_unique<SendThread> (*this);
    sendThread->startThread (juce::Thread::Priority::high);
    netThread = std::make_unique<NetThread> (*this);
    netThread->startThread (juce::Thread::Priority::high);
}

void SendStream::setRingChannels (int numChannels)
{
    if (numChannels <= 0 || numChannels == ringNumChannels) return;
    prepareToPlay (static_cast<double> (ringSampleRate), ringBlockSize,
                   numChannels, wireFormat.load (std::memory_order_acquire));
}

void SendStream::releaseResources()
{
    running.store (false, std::memory_order_release);
    wake.signal();
    if (sendThread) { sendThread->stopThread (400); sendThread.reset(); }
    if (netThread)  { netThread->stopThread  (400); netThread.reset();  }

    if (socketBound)
    {
        socket.reset();
        socketBound = false;
        listenPort = 0;
    }
    fifo.reset();
    ringBuffer.clear();
    drainBuffer.clear();
    netBuffer.clear();
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------

bool SendStream::pushAudioBlock (const float* const* channelData,
                                 int numChannels, int numSamples)
{
    const juce::ScopedTryLock sl (reconfigLock);
    if (! sl.isLocked()) return false;

    if (! fifo || numSamples <= 0 || channelData == nullptr) return false;

    const int chans = juce::jmin (numChannels, ringNumChannels);
    if (chans <= 0) return false;

    const int needed = numSamples * ringNumChannels;

    int start1, size1, start2, size2;
    fifo->prepareToWrite (needed, start1, size1, start2, size2);
    const int actuallyWritable = size1 + size2;
    if (actuallyWritable < needed)
    {
        fifo->finishedWrite (0);
        audioOverruns.fetch_add (1, std::memory_order_relaxed);
        wake.signal();
        return false;
    }

    auto interleaveInto = [&] (int dstStart, int dstFrames)
    {
        float* dst = ringBuffer.data() + dstStart;
        for (int f = 0; f < dstFrames; ++f)
        {
            for (int ch = 0; ch < chans; ++ch)
                dst[f * ringNumChannels + ch] = channelData[ch][f];
            for (int ch = chans; ch < ringNumChannels; ++ch)
                dst[f * ringNumChannels + ch] = 0.0f;
        }
    };

    const int frames1 = size1 / ringNumChannels;
    interleaveInto (start1, frames1);
    if (size2 > 0)
    {
        int dst = start2;
        for (int f = frames1; f < numSamples; ++f)
        {
            for (int ch = 0; ch < chans; ++ch)
                ringBuffer[static_cast<size_t> (dst + (f - frames1) * ringNumChannels + ch)]
                    = channelData[ch][f];
            for (int ch = chans; ch < ringNumChannels; ++ch)
                ringBuffer[static_cast<size_t> (dst + (f - frames1) * ringNumChannels + ch)]
                    = 0.0f;
        }
    }

    fifo->finishedWrite (needed);
    wake.signal();
    return true;
}

// ---------------------------------------------------------------------------
// Target management
// ---------------------------------------------------------------------------

bool SendStream::addTarget (const juce::String& host, int port)
{
    if (host.isEmpty() || port < 1 || port > 65535) return false;

    sockaddr_in addr {};
    if (! resolveIPv4 (host, port, addr)) return false;

    {
        const juce::ScopedLock sl (targetsLock);

        // Idempotent: dedupe by host:port. Re-adding an existing target
        // re-arms the INVITE retry (useful for nudging a peer that
        // didn't ACK the first round).
        // Reset addedAt + lastInviteAt to NOW on every (re-)add so the
        // retry timer doesn't immediately Timeout an entry whose
        // original addedAt is from a previous connect session — that
        // bug made reconnects appear to hang in Inviting.
        const auto nowT = juce::Time::getCurrentTime();
        for (auto& t : targets)
        {
            if (t.host == host && t.port == port)
            {
                t.state         = TargetState::Pending;
                t.rejectReason  = IAR_OK;
                t.addedAt       = nowT;
                t.lastInviteAt  = nowT;
                sendOneInvite (t);
                return true;
            }
        }

        if ((int) targets.size() >= kMaxTargets)
        {
            DBG ("mcfx_send: target list full (cap " << kMaxTargets << "), refusing add");
            return false;
        }

        Target t;
        t.addr        = addr;
        t.host        = host;
        t.port        = port;
        t.label       = host + ":" + juce::String (port);
        t.state       = TargetState::Pending;
        t.addedAt     = juce::Time::getCurrentTime();
        t.lastInviteAt = t.addedAt;
        targets.push_back (t);
        sendOneInvite (targets.back());
    }
    wake.signal();
    return true;
}

void SendStream::removeTarget (const juce::String& host, int port)
{
    Target evicted;
    bool found = false;
    {
        const juce::ScopedLock sl (targetsLock);
        for (auto it = targets.begin(); it != targets.end(); ++it)
        {
            if (it->host == host && it->port == port)
            {
                evicted = *it;
                targets.erase (it);
                found = true;
                break;
            }
        }
    }
    if (found) sendOneUninvite (evicted);
}

void SendStream::clearTargets()
{
    std::vector<Target> evicted;
    {
        const juce::ScopedLock sl (targetsLock);
        evicted.swap (targets);
    }
    for (auto& t : evicted) sendOneUninvite (t);
}

bool SendStream::setSingleTarget (const juce::String& host, int port)
{
    clearTargets();
    if (host.isEmpty() || port <= 0) return true;
    return addTarget (host, port);
}

std::vector<SendStream::Target> SendStream::getTargets() const
{
    const juce::ScopedLock sl (targetsLock);
    return targets;
}

int SendStream::getActiveTargetCount() const noexcept
{
    const juce::ScopedLock sl (targetsLock);
    int n = 0;
    for (auto& t : targets) if (t.state == TargetState::Active) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// Password
// ---------------------------------------------------------------------------

void SendStream::setPassword (const juce::String& pw)
{
    const juce::ScopedLock sl (passwordLock);
    password = pw;
}

juce::String SendStream::getPassword() const
{
    const juce::ScopedLock sl (passwordLock);
    return password;
}

void SendStream::hashPassword (const juce::String& pw, std::uint8_t out[32]) noexcept
{
    if (pw.isEmpty())
    {
        std::memset (out, 0, 32);
        return;
    }
    const auto utf8 = pw.toRawUTF8();
    juce::SHA256 sha (juce::MemoryBlock (utf8, std::strlen (utf8)));
    const auto raw = sha.getRawData();
    jassert (raw.getSize() == 32);
    std::memcpy (out, raw.getData(), 32);
}

bool SendStream::passwordMatches (const std::uint8_t inviterHash[32]) const noexcept
{
    juce::String pw;
    {
        const juce::ScopedLock sl (passwordLock);
        pw = password;
    }
    std::uint8_t mine[32];
    hashPassword (pw, mine);
    return std::memcmp (mine, inviterHash, 32) == 0;
}

// ---------------------------------------------------------------------------
// Send paths
// ---------------------------------------------------------------------------

void SendStream::sendDescriptorTo (const sockaddr_in& addr) noexcept
{
    if (! socket) return;

    uint8_t buf[kDescTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* desc = reinterpret_cast<DescPayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype  = PT_DESC;
    hdr->flags  = 0;
    hdr->sfmt   = static_cast<uint8_t> (wireFormat.load (std::memory_order_acquire));
    hdr->nchan  = static_cast<uint8_t> (ringNumChannels);
    hdr->sender = senderUid;
    hdr->seq    = nextSeq.load (std::memory_order_relaxed);

    desc->fsamp     = static_cast<uint32_t> (ringSampleRate);
    desc->period    = static_cast<uint32_t> (ringBlockSize);
    desc->pktsz     = static_cast<uint32_t> (kDefaultMaxPacketBytes);
    desc->steady_us = static_cast<uint64_t> (
        std::chrono::duration_cast<std::chrono::microseconds> (
            std::chrono::steady_clock::now().time_since_epoch()).count());

    const auto rawHandle = socket->getRawSocketHandle();
    auto sent = ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
                          sizeof (buf), 0,
                          reinterpret_cast<const sockaddr*> (&addr),
                          sizeof (addr));
    if (sent > 0)
    {
        sentPackets.fetch_add (1, std::memory_order_relaxed);
        sentBytes.fetch_add (static_cast<uint64_t> (sent), std::memory_order_relaxed);
    }
}

void SendStream::sendDescriptorToAllActive() noexcept
{
    std::vector<sockaddr_in> snap;
    {
        const juce::ScopedLock sl (targetsLock);
        snap.reserve (targets.size());
        for (auto& t : targets)
            if (t.state == TargetState::Active) snap.push_back (t.addr);
    }
    for (auto& a : snap) sendDescriptorTo (a);
}

void SendStream::sendPeriodFromFifoToAllActive() noexcept
{
    if (! socket || fifo == nullptr) return;

    std::vector<sockaddr_in> snap;
    {
        const juce::ScopedLock sl (targetsLock);
        snap.reserve (targets.size());
        for (auto& t : targets)
            if (t.state == TargetState::Active) snap.push_back (t.addr);
    }

    // Drain one period from the FIFO into drainBuffer regardless of
    // target count — even with zero targets we keep up with the audio
    // thread so the FIFO doesn't overrun.
    const int needed = ringBlockSize * ringNumChannels;
    int start1, size1, start2, size2;
    fifo->prepareToRead (needed, start1, size1, start2, size2);
    if (size1 + size2 < needed) { fifo->finishedRead (0); return; }
    std::memcpy (drainBuffer.data(), ringBuffer.data() + start1,
                 static_cast<size_t> (size1) * sizeof (float));
    if (size2 > 0)
        std::memcpy (drainBuffer.data() + size1,
                     ringBuffer.data() + start2,
                     static_cast<size_t> (size2) * sizeof (float));
    fifo->finishedRead (needed);

    if (snap.empty())
    {
        absoluteFrame.fetch_add (static_cast<uint32_t> (ringBlockSize),
                                 std::memory_order_relaxed);
        return;
    }

    const SampleFormat fmt = wireFormat.load (std::memory_order_acquire);
    const int sampSize = sampleSizeBytes (fmt);
    const int framesPerPkt = framesPerPacket (ringNumChannels, fmt);
    if (framesPerPkt <= 0 || sampSize <= 0) return;

    int frameOffset = 0;
    const uint32_t periodStartFrame = absoluteFrame.load (std::memory_order_relaxed);
    const auto rawHandle = socket->getRawSocketHandle();

    while (frameOffset < ringBlockSize)
    {
        const int frames = juce::jmin (framesPerPkt, ringBlockSize - frameOffset);
        const bool isLast = (frameOffset + frames >= ringBlockSize);

        auto* hdr = reinterpret_cast<CommonHeader*> (netBuffer.data());
        auto* dat = reinterpret_cast<DataPayload*> (netBuffer.data() + sizeof (CommonHeader));

        std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
        hdr->ptype  = PT_DATA;
        hdr->flags  = static_cast<uint8_t> (isLast ? HF_LAST_OF_PERIOD : 0);
        hdr->sfmt   = static_cast<uint8_t> (fmt);
        hdr->nchan  = static_cast<uint8_t> (ringNumChannels);
        hdr->sender = senderUid;
        hdr->seq    = nextSeq.fetch_add (1, std::memory_order_relaxed);

        dat->count    = periodStartFrame + static_cast<uint32_t> (frameOffset);
        dat->nfram    = static_cast<uint16_t> (frames);
        dat->reserved = 0;

        uint8_t* payload = netBuffer.data() + kDataHeaderBytes;
        const int srcOffset = frameOffset * ringNumChannels;
        const int frameBytes = ringNumChannels * sampSize;
        const int totalSamples = frames * ringNumChannels;
        const float* src = drainBuffer.data() + srcOffset;

        if (fmt == SF_FLOAT32)
        {
            std::memcpy (payload, src,
                         static_cast<size_t> (frames) * frameBytes);
        }
        else if (fmt == SF_INT16)
        {
            auto* dst = reinterpret_cast<int16_t*> (payload);
            for (int i = 0; i < totalSamples; ++i)
            {
                const float clipped = juce::jlimit (-1.0f, 1.0f, src[i]);
                dst[i] = static_cast<int16_t> (clipped * 32767.0f);
            }
        }
        else if (fmt == SF_INT24)
        {
            uint8_t* dst = payload;
            constexpr float kInt24Max = 8388607.0f;
            for (int i = 0; i < totalSamples; ++i)
            {
                const float clipped = juce::jlimit (-1.0f, 1.0f, src[i]);
                const int32_t s = static_cast<int32_t> (clipped * kInt24Max);
                dst[0] = static_cast<uint8_t> (s         & 0xff);
                dst[1] = static_cast<uint8_t> ((s >> 8)  & 0xff);
                dst[2] = static_cast<uint8_t> ((s >> 16) & 0xff);
                dst += 3;
            }
        }
        else
        {
            std::memset (payload, 0, static_cast<size_t> (frames) * frameBytes);
        }

        const int totalBytes = kDataHeaderBytes + frames * frameBytes;
        for (auto& addr : snap)
        {
            auto sent = ::sendto (rawHandle,
                                  reinterpret_cast<const char*> (netBuffer.data()),
                                  static_cast<size_t> (totalBytes), 0,
                                  reinterpret_cast<const sockaddr*> (&addr),
                                  sizeof (addr));
            if (sent > 0)
            {
                sentPackets.fetch_add (1, std::memory_order_relaxed);
                sentBytes.fetch_add (static_cast<uint64_t> (sent), std::memory_order_relaxed);
            }
        }

        frameOffset += frames;
    }

    absoluteFrame.fetch_add (static_cast<uint32_t> (ringBlockSize),
                             std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Invite handshake
// ---------------------------------------------------------------------------

void SendStream::sendOneInvite (Target& t) noexcept
{
    if (! socket || ! socketBound) return;

    uint8_t buf[kInviteTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* body = reinterpret_cast<InvitePayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype  = PT_INVITE;
    hdr->flags  = 0;
    hdr->sfmt   = static_cast<uint8_t> (wireFormat.load (std::memory_order_acquire));
    hdr->nchan  = static_cast<uint8_t> (ringNumChannels);
    hdr->sender = senderUid;
    hdr->seq    = nextSeq.load (std::memory_order_relaxed);

    body->listen_port = static_cast<uint32_t> (listenPort);
    {
        juce::String pw;
        {
            const juce::ScopedLock sl (passwordLock);
            pw = password;
        }
        hashPassword (pw, body->pw_hash);
    }

    const auto rawHandle = socket->getRawSocketHandle();
    auto sent = ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
                          sizeof (buf), 0,
                          reinterpret_cast<const sockaddr*> (&t.addr),
                          sizeof (t.addr));
    if (sent > 0)
    {
        sentPackets.fetch_add (1, std::memory_order_relaxed);
        sentBytes.fetch_add (static_cast<uint64_t> (sent), std::memory_order_relaxed);
    }
    t.lastInviteAt = juce::Time::getCurrentTime();
}

void SendStream::sendOneUninvite (const Target& t) noexcept
{
    if (! socket || ! socketBound) return;

    uint8_t buf[kUninviteTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* body = reinterpret_cast<UninvitePayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype  = PT_UNINVITE;
    hdr->sfmt   = static_cast<uint8_t> (wireFormat.load (std::memory_order_acquire));
    hdr->nchan  = static_cast<uint8_t> (ringNumChannels);
    hdr->sender = senderUid;
    hdr->seq    = nextSeq.load (std::memory_order_relaxed);

    body->listen_port = static_cast<uint32_t> (listenPort);

    const auto rawHandle = socket->getRawSocketHandle();
    ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
              sizeof (buf), 0,
              reinterpret_cast<const sockaddr*> (&t.addr),
              sizeof (t.addr));
}

void SendStream::sendInviteAck (const sockaddr_in& to,
                                const InviteAckPayload& body) noexcept
{
    if (! socket || ! socketBound) return;
    uint8_t buf[kInviteAckTotalBytes] {};
    auto* hdr  = reinterpret_cast<CommonHeader*> (buf);
    auto* dst  = reinterpret_cast<InviteAckPayload*> (buf + sizeof (CommonHeader));

    std::memcpy (hdr->magic, kMagic, sizeof (kMagic));
    hdr->ptype  = PT_INVITE_ACK;
    hdr->sfmt   = static_cast<uint8_t> (wireFormat.load (std::memory_order_acquire));
    hdr->nchan  = static_cast<uint8_t> (ringNumChannels);
    hdr->sender = senderUid;
    hdr->seq    = nextSeq.load (std::memory_order_relaxed);
    *dst = body;

    const auto rawHandle = socket->getRawSocketHandle();
    ::sendto (rawHandle, reinterpret_cast<const char*> (buf),
              sizeof (buf), 0,
              reinterpret_cast<const sockaddr*> (&to),
              sizeof (to));
}

void SendStream::tickInviteRetries() noexcept
{
    const auto nowT = juce::Time::getCurrentTime();
    std::vector<Target*> toRetry;
    std::vector<size_t>  toErase;
    // Snapshot under lock, mutate under lock too — but sendOneInvite may
    // do a blocking sendto, so don't hold the lock during it. Two passes.
    {
        const juce::ScopedLock sl (targetsLock);
        for (size_t i = 0; i < targets.size(); ++i)
        {
            auto& t = targets[i];
            if (t.state == TargetState::Pending)
            {
                const auto sinceAdded = (nowT - t.addedAt).inMilliseconds();
                const auto sinceLastInvite = (nowT - t.lastInviteAt).inMilliseconds();
                if (sinceAdded > kInviteDeadlineMs)
                {
                    t.state = TargetState::Timeout;
                    DBG ("mcfx_send: target " << t.label << " INVITE timeout");
                }
                else if (sinceLastInvite > kInviteRetryMs)
                {
                    sendOneInvite (t); // updates t.lastInviteAt
                }
            }
            else if (t.state == TargetState::Rejected
                     || t.state == TargetState::Timeout)
            {
                const auto sinceAdded = (nowT - t.addedAt).inMilliseconds();
                if (sinceAdded > (kInviteDeadlineMs + kRejectKeepMs))
                    toErase.push_back (i);
            }
        }
        // Erase from back to front to keep indices valid.
        for (auto it = toErase.rbegin(); it != toErase.rend(); ++it)
            targets.erase (targets.begin() + *it);
    }
}

// ---------------------------------------------------------------------------
// Inbound control packet handling (NetThread)
// ---------------------------------------------------------------------------

void SendStream::handleInvitePacket (const sockaddr_in& from,
                                     const CommonHeader& hdr,
                                     const InvitePayload& body) noexcept
{
    const bool ok = passwordMatches (body.pw_hash);
    InviteAckPayload ack {};
    ack.accepted          = ok ? 1 : 0;
    ack.reason            = ok ? IAR_OK : IAR_BAD_PASSWORD;
    ack.receiver_jitter_ms = 0; // sender doesn't have a jitter buffer
    ack.receiver_period   = static_cast<uint32_t> (ringBlockSize);

    // Build the inviter's audio target sockaddr — IP from the UDP source,
    // port from the INVITE payload. They might differ (NAT, port-mapper)
    // but on a LAN they usually match.
    sockaddr_in inviterAudioAddr = from;
    inviterAudioAddr.sin_port = htons (static_cast<uint16_t> (body.listen_port));

    if (! ok)
    {
        DBG ("mcfx_send: INVITE rejected (bad password) from uid="
             << juce::String::toHexString ((juce::int64) hdr.sender));
        sendInviteAck (from, ack);
        return;
    }

    // Resolve the inviter's IP into a string so we can dedupe + label.
    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &inviterAudioAddr.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (body.listen_port);

    bool added = false;
    bool full  = false;
    {
        const juce::ScopedLock sl (targetsLock);

        for (auto& t : targets)
        {
            if (t.host == hostStr && t.port == portInt)
            {
                // Already known — refresh state to Active.
                t.state         = TargetState::Active;
                t.receiverUid   = hdr.sender;
                added = true;
                break;
            }
        }
        if (! added)
        {
            if ((int) targets.size() >= kMaxTargets)
            {
                full = true;
            }
            else
            {
                Target t;
                t.addr        = inviterAudioAddr;
                t.host        = hostStr;
                t.port        = portInt;
                t.label       = hostStr + ":" + juce::String (portInt);
                t.receiverUid = hdr.sender;
                t.state       = TargetState::Active;
                t.addedAt     = juce::Time::getCurrentTime();
                t.lastInviteAt = t.addedAt;
                targets.push_back (t);
                added = true;
            }
        }
    }

    if (full)
    {
        ack.accepted = 0;
        ack.reason   = IAR_MAX_TARGETS;
        DBG ("mcfx_send: INVITE rejected (max targets) from uid="
             << juce::String::toHexString ((juce::int64) hdr.sender));
    }
    else
    {
        DBG ("mcfx_send: INVITE accepted from " << hostStr << ":" << portInt
             << " (uid=" << juce::String::toHexString ((juce::int64) hdr.sender) << ")");
    }
    sendInviteAck (from, ack);
}

void SendStream::handleUninvitePacket (const sockaddr_in& from,
                                       const CommonHeader& /*hdr*/,
                                       const UninvitePayload& body) noexcept
{
    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &from.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (body.listen_port);

    {
        const juce::ScopedLock sl (targetsLock);
        for (auto it = targets.begin(); it != targets.end(); ++it)
        {
            if (it->host == hostStr && it->port == portInt)
            {
                DBG ("mcfx_send: UNINVITE from " << hostStr << ":" << portInt);
                targets.erase (it);
                return;
            }
        }
    }
}

void SendStream::handleInviteAckPacket (const sockaddr_in& from,
                                        const CommonHeader& hdr,
                                        const InviteAckPayload& body) noexcept
{
    // A receiver responded to *our* INVITE (i.e. we are the inviter, the
    // receiver is the recipient). Match by source IP — we can't match by
    // listen_port because INVITE_ACK doesn't carry one (the response
    // address IS the receiver's listen address since we sent there).
    char ipbuf[INET_ADDRSTRLEN] {};
    ::inet_ntop (AF_INET, &from.sin_addr, ipbuf, sizeof (ipbuf));
    const juce::String hostStr (ipbuf);
    const int portInt = static_cast<int> (ntohs (from.sin_port));

    const juce::ScopedLock sl (targetsLock);
    for (auto& t : targets)
    {
        if (t.host == hostStr && t.port == portInt)
        {
            t.receiverUid   = hdr.sender;
            t.receiverJitterMs = static_cast<int> (body.receiver_jitter_ms);
            t.receiverPeriod   = static_cast<int> (body.receiver_period);
            t.receiverSampleRate = ringSampleRate; // assume same SR for v1
            if (body.accepted == 1)
            {
                t.state = TargetState::Active;
                DBG ("mcfx_send: ACK accepted by " << t.label);
            }
            else
            {
                t.state = TargetState::Rejected;
                t.rejectReason = static_cast<InviteAckReason> (body.reason);
                DBG ("mcfx_send: ACK rejected by " << t.label
                     << " reason=" << (int) body.reason);
            }
            return;
        }
    }
    // Unknown source — ignore (could be a stale ACK after we cleared targets).
}

}} // namespace mcfx::net
