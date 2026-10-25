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

#include <atomic>

#if defined(__APPLE__)
  #include <dispatch/dispatch.h>
  #include <mach/mach.h>
  #include <mach/mach_time.h>
  #include <mach/thread_act.h>
  #include <mach/thread_policy.h>
  #include <pthread.h>
#elif defined(__linux__)
  #include <cerrno>
  #include <ctime>
  #include <pthread.h>
  #include <sched.h>
  #include <semaphore.h>
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

// Wakes one waiting thread; signal() is safe on the audio thread.
// juce::WaitableEvent::signal() takes a mutex, which the waiter may hold.
// Here signal() is an atomic exchange plus, at most once per wait, an OS
// semaphore post (dispatch / futex / kernel semaphore), none of which
// take a user-space lock. Signals between waits coalesce into one wake.
class RtWake
{
public:
    RtWake()
    {
       #if defined(__APPLE__)
        sem_ = dispatch_semaphore_create (0);
       #elif defined(__linux__)
        sem_init (&sem_, 0, 0);
       #elif defined(_WIN32)
        sem_ = CreateSemaphoreW (nullptr, 0, 0x7fffffff, nullptr);
       #endif
    }

    ~RtWake()
    {
       #if defined(__APPLE__)
        // libdispatch traps on releasing a semaphore whose count is below
        // its initial value, never above it, so leftover posts are fine.
        dispatch_release (sem_);
       #elif defined(__linux__)
        sem_destroy (&sem_);
       #elif defined(_WIN32)
        CloseHandle (sem_);
       #endif
    }

    void signal() noexcept
    {
        // acq_rel: the waiter's exchange in wait() acquires whatever was
        // written before this call (the FIFO write).
        if (! pending_.exchange (true, std::memory_order_acq_rel))
            post();
    }

    // Returns after a signal() or timeoutMs, whichever comes first.
    void wait (int timeoutMs) noexcept
    {
        // Always wait, even with the flag already set: every false->true
        // flip posted once, and that post must be consumed here (it returns
        // at once). Skipping it left the count growing, and later waits
        // returned immediately instead of sleeping.
        timedWait (timeoutMs);
        // Re-arm. A signal() that raced in between is not lost: either it
        // posted (the next wait returns at once) or it found the flag set
        // and this exchange acquires its writes.
        pending_.exchange (false, std::memory_order_acq_rel);
    }

private:
    void post() noexcept
    {
       #if defined(__APPLE__)
        dispatch_semaphore_signal (sem_);
       #elif defined(__linux__)
        sem_post (&sem_);
       #elif defined(_WIN32)
        ReleaseSemaphore (sem_, 1, nullptr);
       #endif
    }

    void timedWait (int timeoutMs) noexcept
    {
       #if defined(__APPLE__)
        dispatch_semaphore_wait (sem_, dispatch_time (DISPATCH_TIME_NOW,
                                                      (int64_t) timeoutMs * NSEC_PER_MSEC));
       #elif defined(__linux__)
        timespec ts {};
        clock_gettime (CLOCK_REALTIME, &ts);
        ts.tv_sec  += timeoutMs / 1000;
        ts.tv_nsec += (long) (timeoutMs % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) { ++ts.tv_sec; ts.tv_nsec -= 1000000000L; }
        while (sem_timedwait (&sem_, &ts) == -1 && errno == EINTR) {}
       #elif defined(_WIN32)
        WaitForSingleObject (sem_, (DWORD) timeoutMs);
       #else
        juce::Thread::sleep (timeoutMs);
       #endif
    }

    std::atomic<bool> pending_ { false };

   #if defined(__APPLE__)
    dispatch_semaphore_t sem_;
   #elif defined(__linux__)
    sem_t sem_;
   #elif defined(_WIN32)
    HANDLE sem_;
   #endif

    JUCE_DECLARE_NON_COPYABLE (RtWake)
};

}} // namespace mcfx::net
