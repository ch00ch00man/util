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

#if defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)

#include "thekogans/util/Exception.h"
#include "thekogans/util/LoggerMgr.h"
#include "thekogans/util/Directory.h"
#include "thekogans/util/XlibUtils.h"

namespace thekogans {
    namespace util {

        DisplayGuard::DisplayGuard (Display *display_) :
                display (display_) {
            if (display != 0) {
                XLockDisplay (display);
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        DisplayGuard::~DisplayGuard () {
            XUnlockDisplay (display);
        }

        std::vector<Display *> EnumerateDisplays () {
            std::vector<Display *> displays;
            Directory directory ("/tmp/.X11-unix");
            Directory::Entry entry;
            for (bool gotEntry = directory.GetFirstEntry (entry);
                    gotEntry; gotEntry = directory.GetNextEntry (entry)) {
                if (!entry.name.empty () && entry.name[0] == 'X') {
                    entry.name[0] = ':';
                    Display * display = XOpenDisplay (entry.name.c_str ());
                    if (display != 0) {
                        displays.push_back (display);
                    }
                    else {
                        THEKOGANS_UTIL_LOG_WARNING (
                            "Unable to open display: %s\n",
                            entry.name.c_str ());
                    }
                }
            }
            return displays;
        }

    } // namespace util
} // namespace thekogans

#endif // defined (TOOLCHAIN_OS_Linux) && defined (THEKOGANS_UTIL_HAVE_XLIB)