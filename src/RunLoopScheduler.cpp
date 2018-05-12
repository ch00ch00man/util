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

#include "thekogans/util/LockGuard.h"
#include "thekogans/util/RunLoopScheduler.h"

namespace thekogans {
    namespace util {

        RunLoopScheduler::JobInfo::Compare RunLoopScheduler::JobInfo::compare;

        THEKOGANS_UTIL_IMPLEMENT_HEAP_WITH_LOCK (RunLoopScheduler::JobInfo, SpinLock)

        void RunLoopScheduler::Queue::Cancel (const RunLoop &runLoop) {
            for (std::size_t i = c.size (); i-- > 0;) {
                if (&c[i]->runLoop == &runLoop) {
                    c.erase (c.begin () + i);
                }
            }
        }

        void RunLoopScheduler::Queue::Cancel (const RunLoop::Job::Id &id) {
            for (std::size_t i = c.size (); i-- > 0;) {
                if (c[i]->job->GetId () == id) {
                    c.erase (c.begin () + i);
                    break;
                }
            }
        }

        void RunLoopScheduler::Cancel (const RunLoop &runLoop) {
            LockGuard<SpinLock> guard (spinLock);
            if (!queue.empty ()) {
                TimeSpec deadline = queue.top ()->deadline;
                queue.Cancel (runLoop);
                if (!queue.empty () && queue.top ()->deadline != deadline) {
                    timer.Start (queue.top ()->deadline - GetCurrentTime ());
                }
            }
        }

        void RunLoopScheduler::Cancel (const RunLoop::Job::Id &id) {
            LockGuard<SpinLock> guard (spinLock);
            if (!queue.empty ()) {
                TimeSpec deadline = queue.top ()->deadline;
                queue.Cancel (id);
                if (!queue.empty () && queue.top ()->deadline != deadline) {
                    timer.Start (queue.top ()->deadline - GetCurrentTime ());
                }
            }
        }

        void RunLoopScheduler::Clear () {
            LockGuard<SpinLock> guard (spinLock);
            timer.Stop ();
            Queue empty;
            queue.swap (empty);
        }

        void RunLoopScheduler::Alarm (Timer & /*timer*/) throw () {
            LockGuard<SpinLock> guard (spinLock);
            TimeSpec now = GetCurrentTime ();
            while (!queue.empty () && queue.top ()->deadline <= now) {
                queue.top ()->EnqJob ();
                queue.pop ();
            }
            if (!queue.empty ()) {
                timer.Start (queue.top ()->deadline - now);
            }
        }

        RunLoop::Job::Id RunLoopScheduler::ScheduleJobInfo (
                JobInfo::Ptr jobInfo,
                const TimeSpec &timeSpec) {
            LockGuard<SpinLock> guard (spinLock);
            bool startTimer = false;
            if (queue.empty () || queue.top ()->deadline > jobInfo->deadline) {
                timer.Stop ();
                startTimer = true;
            }
            queue.push (jobInfo);
            if (startTimer) {
                timer.Start (timeSpec);
            }
            return jobInfo->job->GetId ();
        }

    } // namespace util
} // namespace thekogans
