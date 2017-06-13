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

#include <Foundation/Foundation.h>
#include <string>
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        std::string DescriptionFromOSStatus (OSStatus errorCode) {
            std::string UTF8String;
            NSError *error = [NSError errorWithDomain: NSOSStatusErrorDomain code: errorCode userInfo: nil];
            if (error != 0) {
                NSString *description = [error description];
                if (description != 0) {
                    UTF8String = [description UTF8String];
                    [description release];
                }
                [error release];
            }
            return !UTF8String.empty () ? UTF8String :
                FormatString ("[0x%x:%d - Unknown error.]", errorCode, errorCode);
        }

        namespace {
            struct CFStringRefDeleter {
                void operator () (CFStringRef stringRef) {
                    if (stringRef != 0) {
                        CFRelease (stringRef);
                    }
                }
            };
            typedef std::unique_ptr<const __CFString, CFStringRefDeleter> CFStringRefPtr;
        }

        std::string DescriptionFromCFError (CFErrorRef error) {
            CFStringRefPtr description (CFErrorCopyDescription (error));
            const char *str = 0;
            if (description.get () != 0) {
                str = CFStringGetCStringPtr (description.get (), kCFStringEncodingUTF8);
            }
            return str != 0 ? str : "Unknown error.";
        }

    } // namespace util
} // namespace thekogans