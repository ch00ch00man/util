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

#if defined (TOOLCHAIN_OS_Windows)
    #include <intrin.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/ByteSwap.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/CPUInfo.h"
#include "thekogans/util/RandomSource.h"

namespace thekogans {
    namespace util {

        RandomSource::RandomSource () :
            #if defined (TOOLCHAIN_OS_Windows)
                cryptProv (0) {
            #else // defined (TOOLCHAIN_OS_Windows)
                // http://www.2uo.de/myths-about-urandom/
                urandom (HostEndian, "/dev/urandom") {
            #endif // defined (TOOLCHAIN_OS_Windows)
        #if defined (TOOLCHAIN_OS_Windows)
            if (!CryptAcquireContext (&cryptProv, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE);
            }
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        RandomSource::~RandomSource () {
        #if defined (TOOLCHAIN_OS_Windows)
            CryptReleaseContext (cryptProv, 0);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        namespace {
            // This function was borrowed from:
            // https://stackoverflow.com/questions/11407103/how-i-can-get-the-random-number-from-intels-processor-with-assembler
            ui32 GetHardwareBytes (
                    void *buffer,
                    std::size_t count) {
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                if (CPUInfo::Instance ().RDRAND ()) {
                    std::size_t ui32Count = count >> 2;
                    ui32 *begin = (ui32 *)buffer;
                    ui32 *end = begin + ui32Count;
                    std::size_t safety = ui32Count + 4;
                    ui32 value;
                    while (begin < end && safety > 0) {
                        char rc;
                    #if defined (TOOLCHAIN_OS_Windows)
                        rc = _rdrand32_step (&value);
                    #else // defined (TOOLCHAIN_OS_Windows)
                        __asm__ volatile (
                            "rdrand %0 ; setc %1"
                            : "=r" (value), "=qm" (rc));
                    #endif // defined (TOOLCHAIN_OS_Windows)
                        // 1 = success, 0 = underflow
                        if (rc == 1) {
                            *begin++ = value;
                        }
                        else {
                            --safety;
                        }
                    }
                    if (begin == end) {
                        std::size_t remainder = count & 3;
                        if (remainder > 0) {
                            while (safety > 0) {
                                char rc;
                            #if defined (TOOLCHAIN_OS_Windows)
                                rc = _rdrand32_step (&value);
                            #else // defined (TOOLCHAIN_OS_Windows)
                                __asm__ volatile (
                                    "rdrand %0 ; setc %1"
                                    : "=r" (value), "=qm" (rc));
                            #endif // defined (TOOLCHAIN_OS_Windows)
                                // 1 = success, 0 = underflow
                                if (rc == 1) {
                                    memcpy (begin, &value, remainder);
                                    *((volatile ui32 *)&value) = 0;
                                    return (ui32)count;
                                }
                                else {
                                    --safety;
                                }
                            }
                        }
                    }
                    // Wipe value on exit.
                    *((volatile ui32 *)&value) = 0;
                    return (ui32)((ui8 *)begin - (ui8 *)buffer);
                }
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                return 0;
            }
        }

        ui32 RandomSource::GetBytes (
                void *buffer,
                std::size_t count) {
            if (buffer != 0 && count > 0) {
                LockGuard<SpinLock> guatd (spinLock);
                // If a hardware random source exists, use it first.
                ui32 hardwareCount = GetHardwareBytes (buffer, count);
                if (hardwareCount == count) {
                    return hardwareCount;
                }
                // If we got here either there's no hardware random source or,
                // it couldn't generate enough bytes. Use other means.
            #if defined (TOOLCHAIN_OS_Windows)
                if (!CryptGenRandom (
                        cryptProv,
                        (DWORD)(count - hardwareCount),
                        (BYTE *)buffer + hardwareCount)) {
                    THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (THEKOGANS_UTIL_OS_ERROR_CODE);
                }
                return (ui32)count;
            #else // defined (TOOLCHAIN_OS_Windows)
                return hardwareCount + urandom.Read (
                    (ui8 *)buffer + hardwareCount,
                    (ui32)(count - hardwareCount));
            #endif // defined (TOOLCHAIN_OS_Windows)
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        ui32 RandomSource::Getui32 () {
            ui32 value;
            GetBytes (&value, UI32_SIZE);
            return value;
        }

    } // namespace util
} // namespace thekogans
