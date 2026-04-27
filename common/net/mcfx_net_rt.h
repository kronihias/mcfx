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

 RT-scheduling helper for the mcfx_send / mcfx_receive network threads.

 Promotes the *current* thread to a real-time class so the OS preempts
 user-space work for it. Best-effort — failures are silent (sandbox / lack
 of privilege / overload). The threads keep running with whatever priority
 JUCE assigned, just less reliably.

 Why we need this on top of JUCE Thread::Priority::high:
   - On macOS, JUCE "high" maps to a non-RT QoS class. The audio host
     thread runs in TIME_CONSTRAINT, so on a busy machine our network
     threads can be evicted for tens of ms — long enough to overflow the
     kernel UDP receive queue at high channel counts.
   - On Linux, SCHED_FIFO at moderate priority sits above CFS but below
     the audio thread's typical 80+.
   - On Windows, THREAD_PRIORITY_TIME_CRITICAL is the closest analogue.

 Pattern lifted verbatim from sonolink/Source/RtScheduling.h.

 ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

#if defined(__APPLE__)
  #include <mach/mach.h>
  #include <mach/mach_time.h>
  #include <mach/thread_act.h>
  #include <mach/thread_policy.h>
  #include <pthread.h>
#elif defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
#elif defined(_WIN32)
  #include <windows.h>
#endif

namespace mcfx { namespace net {

// periodMs / computeMs / constraintMs are scheduling hints for macOS — they
// describe the expected duty cycle so the kernel can budget. They're hints,
// not contracts; overruns just cost a preemption. For our network threads
// the threads are NOT the audio thread, so preemptible=true: yield to the
// host's audio thread when it's ready.
inline void setCurrentThreadRealtime (double periodMs,
                                      double computeMs,
                                      double constraintMs) noexcept
{
   #if defined(__APPLE__)
    pthread_set_qos_class_self_np (QOS_CLASS_USER_INTERACTIVE, 0);

    mach_port_t tid = mach_thread_self();

    thread_extended_policy_data_t ext { 0 };
    thread_policy_set (tid, THREAD_EXTENDED_POLICY,
                       reinterpret_cast<thread_policy_t> (&ext),
                       THREAD_EXTENDED_POLICY_COUNT);

    // BASEPRI_FOREGROUND = 47. 52 sits comfortably above app-normal but
    // below the audio thread's typical ~63.
    thread_precedence_policy_data_t prec { 52 };
    thread_policy_set (tid, THREAD_PRECEDENCE_POLICY,
                       reinterpret_cast<thread_policy_t> (&prec),
                       THREAD_PRECEDENCE_POLICY_COUNT);

    mach_timebase_info_data_t tb {};
    mach_timebase_info (&tb);
    const double ms_to_abs = (static_cast<double> (tb.denom) / tb.numer) * 1.0e6;

    thread_time_constraint_policy_data_t tc {};
    tc.period      = static_cast<uint32_t> (periodMs     * ms_to_abs);
    tc.computation = static_cast<uint32_t> (computeMs    * ms_to_abs);
    tc.constraint  = static_cast<uint32_t> (constraintMs * ms_to_abs);
    tc.preemptible = 1;
    thread_policy_set (tid, THREAD_TIME_CONSTRAINT_POLICY,
                       reinterpret_cast<thread_policy_t> (&tc),
                       THREAD_TIME_CONSTRAINT_POLICY_COUNT);

   #elif defined(__linux__)
    juce::ignoreUnused (periodMs, computeMs, constraintMs);
    sched_param sp {};
    sp.sched_priority = 20; // above CFS, below the audio thread's usual SCHED_FIFO 80–95
    pthread_setschedparam (pthread_self(), SCHED_FIFO, &sp);

   #elif defined(_WIN32)
    juce::ignoreUnused (periodMs, computeMs, constraintMs);
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

   #else
    juce::ignoreUnused (periodMs, computeMs, constraintMs);
   #endif
}

}} // namespace mcfx::net
