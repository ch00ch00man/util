// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include <signal.h>
#if defined (TOOLCHAIN_OS_OSX)
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#endif // defined (TOOLCHAIN_OS_OSX)
#endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
#include <cassert>
#include "thekogans/util/Types.h"
#include "thekogans/util/Thread.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/TimeSpec.h"
#if defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/LockGuard.h"
#if defined (TOOLCHAIN_OS_OSX)
    #include "thekogans/util/internal.h"
#endif // defined (TOOLCHAIN_OS_OSX)
#endif // defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)

namespace thekogans {
    namespace util {

    #if !defined (TOOLCHAIN_OS_Windows)
        SpinLock Thread::spinLock;
    #endif // !defined (TOOLCHAIN_OS_Windows)
        Thread::ExitFuncList Thread::exitFuncList;

        Thread::~Thread () {
        #if defined (TOOLCHAIN_OS_Windows)
            if (thread != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                CloseHandle (thread);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (joinable && !joined && exited) {
                Wait (TimeSpec::Zero);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Thread::AddExitFunc (ExitFunc exitFunc) {
            if (exitFunc != 0) {
                exitFuncList.push_back (exitFunc);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        i32 Thread::GetPolicy () {
        #if defined (TOOLCHAIN_OS_Windows)
            return 0;
        #elif defined (TOOLCHAIN_OS_Linux)
            return sched_getscheduler (getpid ());
        #elif defined (TOOLCHAIN_OS_OSX)
            return 0;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        Thread::PriorityRange Thread::GetPriorityRange (i32 policy) {
        #if defined (TOOLCHAIN_OS_Windows)
            return PriorityRange (
                THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
        #elif defined (TOOLCHAIN_OS_Linux) || defined (TOOLCHAIN_OS_OSX)
            return PriorityRange (
                sched_get_priority_min (policy),
                sched_get_priority_max (policy));
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        const char * const Thread::IDLE_THREAD_PRIORITY = "idle";
        const char * const Thread::LOWEST_THREAD_PRIORITY = "lowest";
        const char * const Thread::LOW_THREAD_PRIORITY = "low";
        const char * const Thread::NORMAL_THREAD_PRIORITY = "normal";
        const char * const Thread::HIGH_THREAD_PRIORITY = "high";
        const char * const Thread::HIGHEST_THREAD_PRIORITY = "highest";
        const char * const Thread::REAL_TIME_THREAD_PRIORITY = "real_time";

        std::string Thread::priorityTostring (i32 priority) {
            return
                priority == THEKOGANS_UTIL_IDLE_THREAD_PRIORITY ? IDLE_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY ? LOWEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_LOW_THREAD_PRIORITY ? LOW_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY ? NORMAL_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGH_THREAD_PRIORITY ? HIGH_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY ? HIGHEST_THREAD_PRIORITY :
                priority == THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY ? REAL_TIME_THREAD_PRIORITY :
                i32Tostring (priority);
        }

        i32 Thread::stringToPriority (const std::string &priority) {
            return
                priority == IDLE_THREAD_PRIORITY ? THEKOGANS_UTIL_IDLE_THREAD_PRIORITY :
                priority == LOWEST_THREAD_PRIORITY ? THEKOGANS_UTIL_LOWEST_THREAD_PRIORITY :
                priority == LOW_THREAD_PRIORITY ? THEKOGANS_UTIL_LOW_THREAD_PRIORITY :
                priority == NORMAL_THREAD_PRIORITY ? THEKOGANS_UTIL_NORMAL_THREAD_PRIORITY :
                priority == HIGH_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGH_THREAD_PRIORITY :
                priority == HIGHEST_THREAD_PRIORITY ? THEKOGANS_UTIL_HIGHEST_THREAD_PRIORITY :
                priority == REAL_TIME_THREAD_PRIORITY ? THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY :
                stringToi32 (priority.c_str ());
        }

        void Thread::Create (
                i32 priority,
                ui32 affinity) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (thread != THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                if (!CloseHandle (thread)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_OS_ERROR_CODE);
                }
            }
            thread = CreateThread (0, 0, ThreadProc, this, 0, 0);
            if (thread == THEKOGANS_UTIL_INVALID_HANDLE_VALUE) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            // If we're recycling threads and Wait was not
            // called on the old thread, reap it here.
            if (joinable && !joined && exited) {
                Wait (TimeSpec::Zero);
            }
            {
                // Signals and threads are like oil and water.
                // They just don't mix. Block all signals in
                // threads. If you want to handle signals, do
                // so in the main thread.
                struct BlockSignals {
                    LockGuard<SpinLock> guard;
                    sigset_t oldset;
                    explicit BlockSignals (SpinLock &spinLock) :
                            guard (spinLock) {
                        sigset_t set;
                        sigfillset (&set);
                        int result = pthread_sigmask (SIG_SETMASK, &set, &oldset);
                        if (result != 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                        }
                    }
                    ~BlockSignals () {
                        pthread_sigmask (SIG_SETMASK, &oldset, 0);
                    }
                } volatile blockSignals (spinLock);
                if (!joinable) {
                    struct Attribute {
                        pthread_attr_t attribute;
                        Attribute () {
                            int result = pthread_attr_init (&attribute);
                            if (result != 0) {
                                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                            }
                        }
                        ~Attribute () {
                            pthread_attr_destroy (&attribute);
                        }
                    } attribute;
                    int result =
                        pthread_attr_setdetachstate (
                            &attribute.attribute,
                            PTHREAD_CREATE_DETACHED);
                    if (result == 0) {
                        result = pthread_create (&thread, &attribute.attribute, ThreadProc, this);
                        if (result != 0) {
                            THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                        }
                    }
                    else {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                    }
                }
                else {
                    int result = pthread_create (&thread, 0, ThreadProc, this);
                    if (result != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                    }
                }
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
            SetThreadPriority (thread, priority);
            if (affinity != UI32_MAX) {
                SetThreadAffinity (thread, affinity);
            }
        }

        bool Thread::Wait (const TimeSpec &timeSpec) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD result = WaitForSingleObject (thread, (DWORD)timeSpec.ToMilliseconds ());
            if (result != WAIT_OBJECT_0) {
                if (result == WAIT_TIMEOUT || result == WAIT_ABANDONED) {
                    return false;
                }
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
            return true;
        #else // defined (TOOLCHAIN_OS_Windows)
            if (joinable && !joined) {
                joined = true;
                if (timeSpec == TimeSpec::Infinite) {
                    int result = pthread_join (thread, 0);
                    if (result != 0) {
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                    }
                }
                else {
                    timespec absolute = (GetCurrentTime () + timeSpec).Totimespec ();
                    int result = pthread_timedjoin_np (thread, 0, &absolute);
                    if (result != 0) {
                        if (result == ETIMEDOUT) {
                            return false;
                        }
                        THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                    }
                }
                return true;
            }
            return false;
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Thread::SetThreadPriority (
                THEKOGANS_UTIL_THREAD_HANDLE thread,
                i32 priority) {
        #if defined (TOOLCHAIN_OS_Windows)
            if (!::SetThreadPriority (thread, priority)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #else // defined (TOOLCHAIN_OS_Windows)
            if (priority >= THEKOGANS_UTIL_IDLE_THREAD_PRIORITY &&
                    priority <= THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY) {
                int policy;
                sched_param param;
                int result = pthread_getschedparam (thread, &policy, &param);
                if (result != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                }
                // Our priorities come from a virtual range:
                // (THEKOGANS_UTIL_IDLE_THREAD_PRIORITY <= priority <= THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY).
                // We need to map them to the range supported
                // by the particular scheduler policy.
                int minPriority = sched_get_priority_min (policy);
                int maxPriority = sched_get_priority_max (policy);
                f32 scale = ((f32)maxPriority - (f32)minPriority) /
                    ((f32)THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY - (f32)THEKOGANS_UTIL_IDLE_THREAD_PRIORITY);
                param.sched_priority = minPriority + scale * priority;
                result = pthread_setschedparam (thread, policy, &param);
                if (result != 0) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                    "Priority %d is out of range (%d, %d).",
                    priority,
                    THEKOGANS_UTIL_IDLE_THREAD_PRIORITY,
                    THEKOGANS_UTIL_REAL_TIME_THREAD_PRIORITY);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Thread::SetThreadAffinity (
                THEKOGANS_UTIL_THREAD_HANDLE thread,
                ui32 affinity) {
        #if defined (TOOLCHAIN_OS_Windows)
            DWORD affinityMask = 1 << affinity;
            if (SetThreadAffinityMask (thread, affinityMask) == 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #elif defined (TOOLCHAIN_OS_Linux)
            cpu_set_t affinityMask;
            CPU_ZERO (&affinityMask);
            CPU_SET (affinity, &affinityMask);
            int result = pthread_setaffinity_np (thread, sizeof (affinityMask), &affinityMask);
            if (result != 0) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (result);
            }
        #elif defined (TOOLCHAIN_OS_OSX)
            thread_affinity_policy_data_t policy;
            policy.affinity_tag = affinity;
            kern_return_t result = thread_policy_set (
                pthread_mach_thread_np (thread),
                THREAD_AFFINITY_POLICY,
                reinterpret_cast<thread_policy_t> (&policy),
                THREAD_AFFINITY_POLICY_COUNT);
            if (result != KERN_SUCCESS) {
                THEKOGANS_UTIL_THROW_MACH_ERROR_CODE_EXCEPTION (result);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        void Thread::Pause (ui32 count) {
        #if defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_ARCH_i386)
            _asm {
                mov eax, count
            }
            pauseLoop:
            _asm {
                pause
                add eax, -1
                jne pauseLoop
            }
        #else // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_ARCH_i386)
            while (count-- > 0) {}
        #endif // defined (TOOLCHAIN_OS_Windows) && defined (TOOLCHAIN_ARCH_i386)
        }

        void Thread::YieldSlice () {
        #if defined (TOOLCHAIN_OS_Windows)
            SwitchToThread ();
        #else // defined (TOOLCHAIN_OS_Windows)
            sched_yield ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    #if defined (TOOLCHAIN_OS_Windows)
        DWORD WINAPI Thread::ThreadProc (LPVOID data) {
    #else // defined (TOOLCHAIN_OS_Windows)
        void *Thread::ThreadProc (void *data) {
    #endif // defined (TOOLCHAIN_OS_Windows)
            assert (data != 0);
            Thread *thread = static_cast<Thread *> (data);
            if (!thread->name.empty ()) {
            #if defined (TOOLCHAIN_OS_Windows)
                // This code came from: https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
            #pragma pack (push, 8)
                struct THREADNAME_INFO {
                    DWORD dwType; // Must be 0x1000.
                    LPCSTR szName; // Pointer to name (in user addr space).
                    DWORD dwThreadID; // Thread ID (-1 = caller thread).
                    DWORD dwFlags; // Reserved for future use, must be zero.

                    THREADNAME_INFO (
                        const std::string &name,
                        THEKOGANS_UTIL_THREAD_HANDLE thread) :
                        dwType (0x1000),
                        szName (name.c_str ()),
                        dwThreadID (GetThreadId (thread)),
                        dwFlags (0) {}
                } info (thread->name, thread->thread);
            #pragma pack (pop)
            #pragma warning (push)
            #pragma warning (disable: 6320 6322)
                __try {
                    RaiseException (
                        MS_VC_EXCEPTION,
                        0,
                        sizeof (info) / sizeof (ULONG_PTR),
                        (ULONG_PTR *)&info);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            #pragma warning (pop)
            #elif defined (TOOLCHAIN_OS_OSX)
                pthread_setname_np (thread->name.c_str ());
            #else // defined (TOOLCHAIN_OS_OSX)
                const std::size_t LINUX_MAX_THREAD_NAME = 15;
                char name[LINUX_MAX_THREAD_NAME + 1];
                std::size_t length =
                    std::min (thread->name.size (), LINUX_MAX_THREAD_NAME);
                strncpy (name, thread->name.c_str (), length);
                name[length] = '\0';
                pthread_setname_np (thread->thread, name);
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
        #if !defined (TOOLCHAIN_OS_Windows)
            thread->joined = false;
        #endif // !defined (TOOLCHAIN_OS_Windows)
            thread->exited = false;
            thread->Run ();
            AtExit (thread->GetThreadHandle ());
            thread->exited = true;
            return 0;
        }

        void Thread::AtExit (THEKOGANS_UTIL_THREAD_HANDLE thread) {
            for (std::size_t i = 0, count = exitFuncList.size (); i < count; ++i) {
                exitFuncList[i] (thread);
            }
        }

    } // namespace util
} // namespace thekogans
