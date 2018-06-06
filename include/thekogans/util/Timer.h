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

#if !defined (__thekogans_util_Timer_h)
#define __thekogans_util_Timer_h

#if defined (TOOLCHAIN_OS_Windows)
    #if !defined (_WINDOWS_)
        #if !defined (WIN32_LEAN_AND_MEAN)
            #define WIN32_LEAN_AND_MEAN
        #endif // !defined (WIN32_LEAN_AND_MEAN)
        #if !defined (NOMINMAX)
            #define NOMINMAX
        #endif // !defined (NOMINMAX)
        #include <windows.h>
    #endif // !defined (_WINDOWS_)
#elif defined (TOOLCHAIN_OS_Linux)
    #include <ctime>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"
#include "thekogans/util/TimeSpec.h"
#include "thekogans/util/SpinLock.h"

namespace thekogans {
    namespace util {

        /// \struct Timer Timer.h thekogans/util/Timer.h
        ///
        /// \brief
        /// A millisecond resolution, platform independent, asynchronous
        /// timer. It's use is suitable where accuracy is not paramount
        /// (idle processing). If high resolution timing is what you need,
        /// use HRTimer/HRTimerMgr instead. Here is a typical use case:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// struct IdleProcessor :
        ///         public util::Timer::Callback,
        ///         public util::Singleton<IdleProcessor, util::SpinLock> {
        /// private:
        ///     util::Timer timer;
        ///     util::JobQueue jobQueue;
        ///
        /// public:
        ///     IdleProcessor () :
        ///         timer ("IdleProcessor"),
        ///         jobQueue (
        ///             "IdleProcessor",
        ///             util::RunLoop::TYPE_FIFO,
        ///             util::UI32_MAX,
        ///             1,
        ///             THEKOGANS_UTIL_LOW_THREAD_PRIORITY) {}
        ///
        ///     inline void StartTimer (const util::TimeSpec &timeSpec) {
        ///         timer.Start (timeSpec);
        ///     }
        ///     inline void StopTimer () {
        ///         timer.Stop ();
        ///     }
        ///
        ///     inline void CancelPendingJobs (bool waitForIdle = true) {
        ///         jobQueue.CancelAllJobs ();
        ///         if (waitForIdle) {
        ///             jobQueue.WaitForIdle ();
        ///         }
        ///     }
        ///
        /// private:
        ///     // RefCounted
        ///     virtual void Harakiri () {}
        ///
        ///     // Timer::Callback
        ///     virtual void Alarm (util::Timer & /*timer*/) throw () {
        ///         // queue idle jobs.
        ///         jobQueue.EnqJob (...);
        ///     }
        /// };
        /// \endcode
        ///
        /// In your code you can now write:
        ///
        /// \code{.cpp}
        /// IdleProcessor::Instance ().StartTimer (TimeSpec::FromSeconds (30));
        /// \endcode
        ///
        /// This will arm the IdleProcessor timer to fire after 30 seconds.
        /// Call IdleProcessor::Instance ().StopTimer () to disarm the timer.
        ///
        /// NOTE: Timer is designed to have the same semantics on all supported
        /// platforms. To that end if you're using a periodic timer and it fires
        /// while Timer::Callback::Alarm is in progress, that event will be silently
        /// dropped and that cycle will be missed. There are no 'catchup' events.
        ///
        /// NOTE: IdleProcessor demonstrates the canonical way of using Timer.
        /// By coupling the lifetime of the timer to the lifetime of the callback
        /// (IdleProcessor), we avoid calling callback through a dangling pointer
        /// as every call to Timer::Start takes out a reference on the callback.

        struct _LIB_THEKOGANS_UTIL_DECL Timer {
            /// \struct Timer::Callback Timer.h thekogans/util/Timer.h
            ///
            /// \brief
            /// An abstract base class used to notify timer listeners.
            struct _LIB_THEKOGANS_UTIL_DECL Callback :
                    public virtual thekogans::util::ThreadSafeRefCounted {
                /// \brief
                /// Convenient typedef for thekogans::util::ThreadSafeRefCounted::Ptr<Callback>.
                typedef thekogans::util::ThreadSafeRefCounted::Ptr<Callback> Ptr;

                /// \brief
                /// dtor.
                virtual ~Callback () {}

                /// \brief
                /// Called every time the timer fires.
                /// \param[in] timer Timer that fired.
                virtual void Alarm (Timer & /*timer*/) throw () = 0;
            };

        private:
            /// \brief
            /// Receiver of the alarm notofocations.
            Callback &callback;
            /// \brief
            /// Timer name.
            std::string name;
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Windows native timer object.
            PTP_TIMER timer;
            /// \brief
            /// Windows timer callback.
            static VOID CALLBACK TimerCallback (
                PTP_CALLBACK_INSTANCE /*Instance*/,
                PVOID Context,
                PTP_TIMER /*Timer*/);
        #elif defined (TOOLCHAIN_OS_Linux)
            /// \brief
            /// Linux native timer object.
            timer_t timer;
            /// \brief
            /// Linux timer callback.
            static void TimerCallback (union sigval val);
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// OS X kqueue event id.
            ui64 id;
            enum {
                /// \brief
                /// Default minimum number of \see{JobQueue}s in the pool.
                DEFAULT_MIN_JOB_QUEUE_POOL_JOB_QUEUES = 1,
                /// \brief
                /// Default maximum number of \see{JobQueue}s in the pool.
                DEFAULT_MAX_JOB_QUEUE_POOL_JOB_QUEUES = 100,
            };
            /// \brief
            /// Minimum number of \see{JobQueue}s to keep around.
            static ui32 minJobQueues;
            /// \brief
            /// Maximum number of \see{JobQueue}s the pool can grow to.
            static ui32 maxJobQueues;
            /// \brief
            /// Forward declaration of AlarmJob.
            struct AlarmJob;
            /// \brief
            /// An OS X kqueue monitoring async timers.
            friend struct TimerQueue;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// How long before the timer fires.
            TimeSpec timeSpec;
            /// \brief
            /// true = timer is periodic, false = timer is one shot.
            bool periodic;
            /// \brief
            /// Synchronization lock.
            SpinLock spinLock;
            /// \brief
            /// Periodic Callback::Alarm synchronization lock.
            SpinLock inAlarmSpinLock;

        public:
            /// \brief
            /// ctor.
            /// \param[in] callback_ Callback to notify of timer events.
            /// \param[in] name_ Timer name. Use this parameter to help
            /// identify the timer that fired. This way a single callback
            /// can process multiple timers and be able to distinguish
            /// between them.
            Timer (
                Callback &callback_,
                const std::string &name_ = std::string ());
            /// \brief
            /// dtor.
            ~Timer ();

        #if defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// On OS X Timer uses a kqueue and a \see{JobQueuePool} to service
            /// timers and alarms. If your app uses a lot of timers, firing
            /// often, you might want to adjust the defaults to suit your needs.
            /// IMPORTANT: This function must be called before you create the
            /// first timer.
            /// \param[in] minWorkers_ Minimum number of workers to keep around.
            /// \param[in] maxWorkers_ Maximum number of workers the pool can grow to.
            static void SetJobQueuePoolMinMaxJobQueues (
                ui32 minJobQueues_ = DEFAULT_MIN_JOB_QUEUE_POOL_JOB_QUEUES,
                ui32 maxJobQueues_ = DEFAULT_MAX_JOB_QUEUE_POOL_JOB_QUEUES);
        #endif // defined (TOOLCHAIN_OS_OSX)

            /// \brief
            /// Return the timer name.
            /// \return Timer name.
            inline const std::string &GetName () const {
                return name;
            }
            /// brief
            /// Set the timer name.
            /// \param[in] name_ Timer name.
            inline void SetName (const std::string &name_) {
                name = name_;
            }

            /// \brief
            /// Start the timer. If the timer is already running
            /// it will be rearmed with the new parameters.
            /// \param[in] timeSpec_ How long before the timer fires.
            /// IMPORTANT: This is a relative value.
            /// \param[in] periodic_ true = timer is periodic, false = timer is one shot.
            /// It will fire with the period of timeSpec.
            void Start (
                const TimeSpec &timeSpec_,
                bool periodic_ = false);
            /// \brief
            /// Stop the timer.
            void Stop ();

            /// \brief
            /// Rreturn true if timer is running.
            /// \return true == timer is running.
            bool IsRunning ();

        private:
            /// \brief
            /// Unprotected Stop. Called by both Startand Stop.
            void StopHelper ();

            /// \brief
            /// Timer is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Timer)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Timer_h)
