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

 UDP socket helpers for mcfx_send / mcfx_receive.

 Wraps juce::DatagramSocket so we get JUCE's polymorphic read()/waitUntilReady,
 then reaches through getRawSocketHandle() to apply the OS-level tuning AOO
 didn't set for us:
   * 4 MB SO_SNDBUF / SO_RCVBUF — the kernel UDP queue is the bottleneck at
     high channel counts, NOT bandwidth (see sonolink commits 281d8d9, ec33ea6).
   * DSCP / NET_SERVICE_TYPE — marks audio packets as Voice priority on Wi-Fi
     (WMM AC_VO) and managed networks. Per-platform: SO_NET_SERVICE_TYPE on
     macOS, IP_TOS+SO_PRIORITY on Linux, IP_TOS on Windows.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#if defined(__APPLE__)
  // Forward-declare the CoreFoundation / SystemConfiguration types we
  // need WITHOUT including their headers. <CoreFoundation.h> and
  // <SystemConfiguration.h> both transitively pull in <MacTypes.h>
  // which #defines `Point` and collides with juce::Point at every call
  // site. The opaque CF* types are pointers to anonymous structs, and
  // CFRelease + the Boolean / encoding integers can be forward-declared
  // by hand.
  typedef const struct __CFString* CFStringRef;
  typedef const void* CFTypeRef;
  typedef struct __SCDynamicStore* SCDynamicStoreRef;
  typedef unsigned long CFIndex;
  typedef unsigned char CFStringBuiltInEncodingsBoolean; // unsigned char
  extern "C" {
    CFStringRef SCDynamicStoreCopyComputerName (SCDynamicStoreRef store,
                                                std::uint32_t* nameEncoding);
    bool        CFStringGetCString (CFStringRef theString, char* buffer,
                                    CFIndex bufferSize, std::uint32_t encoding);
    void        CFRelease (CFTypeRef cf);
  }
  // kCFStringEncodingUTF8 from CFString.h.
  static constexpr std::uint32_t kMcfxCFStringEncodingUTF8 = 0x08000100;
#endif

namespace mcfx { namespace net {

// Returns the user-visible computer name. On macOS this is the Sharing
// pref pane's "Computer Name" (e.g. "Matthias's MacBook Pro"), not the
// unix hostname (which would be something like "Mac" or
// "Matthiass-MacBook-Pro-M2-2"). JUCE's SystemStats::getComputerName()
// uses gethostname() and gets the unix one — we want the friendly name
// for the Bonjour TXT record because that's what the user recognises.
//
// macOS API: SCDynamicStoreCopyComputerName lives in
// SystemConfiguration.framework, which juce_audio_devices already links
// for CoreAudio device enumeration. No new build dep.
inline juce::String getFriendlyComputerName()
{
   #if defined(__APPLE__)
    if (CFStringRef name = SCDynamicStoreCopyComputerName (nullptr, nullptr))
    {
        char buf[256] = {};
        const bool ok = CFStringGetCString (name, buf, sizeof (buf),
                                            kMcfxCFStringEncodingUTF8);
        CFRelease (name);
        if (ok && buf[0] != '\0')
            return juce::String (juce::CharPointer_UTF8 (buf));
    }
   #endif
    // Fallback for non-macOS or if SC returned nothing (rare).
    return juce::SystemStats::getComputerName();
}

// Returns this machine's IPv4 interface addresses in dotted-quad form,
// minus 127.0.0.1. Order is whatever the OS reports — typically the
// active LAN interface comes first. Used by the editors' "Listening on…"
// label so a user can read off the address to give to a peer connecting
// via Direct IP.
inline juce::StringArray getLocalIPv4Addresses()
{
    juce::StringArray out;
    for (const auto& a : juce::IPAddress::getAllAddresses (/*includeIPv6=*/false))
    {
        const auto s = a.toString();
        if (s.isEmpty() || s == "127.0.0.1") continue;
        out.addIfNotAlreadyThere (s);
    }
    return out;
}

}} // namespace mcfx::net

namespace mcfx { namespace net {

// Bumped buffer size for both directions. macOS kern.ipc.maxsockbuf default
// is 8 MB so 4 MB is safe to request; the granted size may be smaller and
// is logged via the JUCE DBG channel.
inline constexpr int kKernelUdpBufBytes = 4 * 1024 * 1024;

// Try to expand the kernel UDP send/recv buffers. Best-effort.
inline void tuneKernelUdpBuffers (int rawHandle, const char* tag = "mcfx_net") noexcept
{
    auto setBuf = [rawHandle] (int opt) {
       #ifdef _WIN32
        ::setsockopt (static_cast<SOCKET> (rawHandle), SOL_SOCKET, opt,
                      reinterpret_cast<const char*> (&kKernelUdpBufBytes),
                      sizeof (kKernelUdpBufBytes));
       #else
        ::setsockopt (rawHandle, SOL_SOCKET, opt,
                      &kKernelUdpBufBytes, sizeof (kKernelUdpBufBytes));
       #endif
    };
    [[maybe_unused]] auto getBuf = [rawHandle] (int opt) -> int {
        int actual = 0;
        socklen_t actualLen = sizeof (actual);
       #ifdef _WIN32
        ::getsockopt (static_cast<SOCKET> (rawHandle), SOL_SOCKET, opt,
                      reinterpret_cast<char*> (&actual), &actualLen);
       #else
        ::getsockopt (rawHandle, SOL_SOCKET, opt, &actual, &actualLen);
       #endif
        return actual;
    };

    setBuf (SO_SNDBUF);
    setBuf (SO_RCVBUF);
    DBG (juce::String (tag) + ": UDP buffer requested=" + juce::String (kKernelUdpBufBytes)
         + " SNDBUF granted=" + juce::String (getBuf (SO_SNDBUF))
         + " RCVBUF granted=" + juce::String (getBuf (SO_RCVBUF)));
    juce::ignoreUnused (tag);
}

// Mark egress packets as high-priority.
inline void applyAudioQos (int rawHandle, const char* tag = "mcfx_net") noexcept
{
   #if defined(__APPLE__)
    const int svc = NET_SERVICE_TYPE_VO;
    [[maybe_unused]] int rc = ::setsockopt (rawHandle, SOL_SOCKET, SO_NET_SERVICE_TYPE,
                                            &svc, sizeof (svc));
    DBG (juce::String (tag) + ": SO_NET_SERVICE_TYPE=VO rc=" + juce::String (rc));
   #elif defined(_WIN32)
    // IP_TOS without elevation is typically ignored on Windows, but
    // costs nothing to try. The proper fix would be qWave (qos2.h),
    // which adds a build dependency we don't want today.
    const DWORD tos = 0xB8; // DSCP-EF
    [[maybe_unused]] int rc = ::setsockopt (static_cast<SOCKET> (rawHandle), IPPROTO_IP, IP_TOS,
                                            reinterpret_cast<const char*> (&tos), sizeof (tos));
    DBG (juce::String (tag) + ": IP_TOS=0xB8 rc=" + juce::String (rc));
   #else
    const int tos = 0xB8; // DSCP 46 (Expedited Forwarding) << 2
    [[maybe_unused]] int rc4 = ::setsockopt (rawHandle, IPPROTO_IP,   IP_TOS,      &tos, sizeof (tos));
    [[maybe_unused]] int rc6 = -1;
   #ifdef IPV6_TCLASS
    rc6 = ::setsockopt (rawHandle, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof (tos));
   #endif
    [[maybe_unused]] int rcp = -1;
   #ifdef SO_PRIORITY
    const int prio = 6; // 802.1q VO
    rcp = ::setsockopt (rawHandle, SOL_SOCKET, SO_PRIORITY, &prio, sizeof (prio));
   #endif
    DBG (juce::String (tag) + ": IP_TOS=0xB8 v4rc=" + juce::String (rc4)
         + " v6rc=" + juce::String (rc6)
         + " SO_PRIORITY=6 rc=" + juce::String (rcp));
   #endif
    juce::ignoreUnused (tag);
}

// (Multicast helpers removed — mcfx_net is single-cast unicast only.)

// Resolve "host:port" into a sockaddr_in for IPv4. Returns false on failure.
// IP literals are the fast path; hostnames go through getaddrinfo. We only
// support IPv4 for v1 (matches sonolink's Endpoint::resolve).
inline bool resolveIPv4 (const juce::String& host, int port,
                         sockaddr_in& out) noexcept
{
    std::memset (&out, 0, sizeof (out));
    out.sin_family = AF_INET;
    out.sin_port   = htons (static_cast<uint16_t> (port));

    addrinfo hints {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo (host.toRawUTF8(), juce::String (port).toRawUTF8(),
                       &hints, &res) != 0 || res == nullptr)
        return false;
    std::memcpy (&out, res->ai_addr, sizeof (out));
    ::freeaddrinfo (res);
    return true;
}

}} // namespace mcfx::net
