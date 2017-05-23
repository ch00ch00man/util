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
#include "thekogans/util/JobQueueScheduler.h"

namespace thekogans {
    namespace util {

        void JobQueueScheduler::Schedule (
                JobQueue &jobQueue,
                JobQueue::Job::UniquePtr job,
                const TimeSpec &timeSpec) {
            LockGuard<SpinLock> guard (spinLock);
            bool startTimer = false;
            TimeSpec now = GetCurrentTime ();
            JobInfo::SharedPtr jobInfo (new JobInfo (jobQueue, std::move (job), now + timeSpec));
            if (queue.empty () || queue.top ()->deadline > jobInfo->deadline) {
                timer.Stop ();
                startTimer = true;
            }
            queue.push (jobInfo);
            if (startTimer) {
                timer.Start (timeSpec);
            }
        }

        void JobQueueScheduler::Alarm (Timer & /*timer*/) throw () {
            LockGuard<SpinLock> guard (spinLock);
            TimeSpec now = GetCurrentTime ();
            while (!queue.empty () && queue.top ()->deadline >= now) {
                queue.top ()->jobQueue.Enq (std::move (queue.top ()->job));
                queue.pop ();
            }
            if (!queue.empty ()) {
                timer.Start (queue.top ()->deadline - now);
            }
        }

    } // namespace util
} // namespace thekogans
