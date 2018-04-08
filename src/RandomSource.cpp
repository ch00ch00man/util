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
                std::size_t total = 0;
            #if defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                if (CPUInfo::Instance ().RDRAND ()) {
                    std::size_t remainder = count & 3;
                    count &= ~3;
                    std::size_t safety = (count >> 2) + 4;
                    ui32 *ptr = (ui32 *)buffer;
                    ui32 value;
                    while (total < count && safety > 0) {
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
                            *ptr++ = value;
                            total += 4;
                        }
                        else {
                            --safety;
                        }
                    }
                    if (total == count) {
                        count += remainder;
                        while (total < count && safety > 0) {
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
                                memcpy (ptr, &value, remainder);
                                total += remainder;
                            }
                            else {
                                --safety;
                            }
                        }
                    }
                    // Wipe value on exit.
                    *((volatile ui32 *)&value) = 0;
                }
            #endif // defined (TOOLCHAIN_ARCH_i386) || defined (TOOLCHAIN_ARCH_x86_64)
                return (ui32)total;
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
