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

#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "thekogans/util/Array.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/LockGuard.h"
#include "thekogans/util/Exception.h"
#include "thekogans/util/XMLUtils.h"
#if defined (TOOLCHAIN_OS_Windows)
    #include "thekogans/util/WindowsUtils.h"
#endif // defined (TOOLCHAIN_OS_Windows)
#include "thekogans/util/StringUtils.h"

namespace thekogans {
    namespace util {

        _LIB_THEKOGANS_UTIL_DECL void _LIB_THEKOGANS_UTIL_API CopyString (
                char *destination,
                std::size_t destinationLength,
                const char *source) {
            if (destination != 0 && destinationLength != 0 && source != 0) {
                while (--destinationLength != 0 && *source != '\0') {
                    *destination++ = *source++;
                }
                *destination = '\0';
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API TrimSpaces (
                const char *str) {
            if (str != 0) {
                while (*str != 0 && isspace (*str)) {
                    ++str;
                }
                for (const char *end = str + strlen (str); end > str; --end) {
                    if (!isspace (*(end - 1))) {
                        return std::string (str, end);
                    }
                }
            }
            return std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToUpper (
                const char *str) {
            std::string upper;
            if (str != 0) {
                while (*str != 0) {
                    upper += toupper (*str++);
                }
            }
            return upper;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API StringToLower (
                const char *str) {
            std::string lower;
            if (str != 0) {
                while (*str != 0) {
                    lower += tolower (*str++);
                }
            }
            return lower;
        }

        namespace {
            const char *hexTable = "0123456789abcdef";
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodeBuffer (
                const void *buffer,
                std::size_t length) {
            std::string hexString;
            if (buffer != 0 && length > 0) {
                hexString.resize (length * 2);
                char *ptr1 = &hexString[0];
                const char *ptr2 = (const char *)buffer;
                for (std::size_t i = 0; i < length; ++i) {
                    *ptr1++ = hexTable[(ptr2[i] & 0xf0) >> 4];
                    *ptr1++ = hexTable[ptr2[i] & 0x0f];
                }
            }
            return hexString;
        }

        namespace {
            inline char DecodeHexChar (char hexChar) {
                return (hexChar >= '0' && hexChar <= '9') ? hexChar - '0' :
                    (hexChar >= 'a' && hexChar <= 'f') ? hexChar - 'a' + 10 :
                    (hexChar >= 'A' && hexChar <= 'F') ? hexChar - 'A' + 10 : -1;
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodeBuffer (
                const char *hexBuffer,
                std::size_t length) {
            std::vector<ui8> buffer;
            if (hexBuffer != 0 && length > 0) {
                if ((length & 1) == 0) {
                    buffer.resize (length / 2);
                    ui8 *ptr = &buffer[0];
                    for (std::size_t i = 0; i < length; i += 2) {
                        *ptr++ = (DecodeHexChar (hexBuffer[i]) << 4) |
                            DecodeHexChar (hexBuffer[i + 1]);
                    }
                }
                else {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "%s", "hexBuffer lenght must be even.");
                }
            }
            return buffer;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexEncodestring (
                const std::string &str) {
            return !str.empty () ? HexEncodeBuffer (&str[0], str.size ()) : std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::vector<ui8> _LIB_THEKOGANS_UTIL_API HexDecodestring (
                const std::string &hexString) {
            return !hexString.empty () ?
                HexDecodeBuffer (&hexString[0], hexString.size ()) :
                std::vector<ui8> ();
        }

        namespace {
            void HexFormatRow (
                    std::ostringstream &stream,
                    const ui8 *ptr,
                    std::size_t columns) {
                for (const ui8 *begin = ptr,
                        *end = ptr + columns; begin != end; ++begin) {
                    const char hex[4] = {
                        hexTable[(*begin & 0xf0) >> 4],
                        hexTable[*begin & 0x0f],
                        ' ',
                        '\0'
                    };
                    stream << hex;
                }
                stream << " ";
            }

            void ASCIIFormatRow (
                    std::ostringstream &stream,
                    const ui8 *ptr,
                    std::size_t columns) {
                for (const ui8 *begin = ptr,
                        *end = ptr + columns; begin != end; ++begin) {
                    const char ch[2] = {
                        isprint (*begin) ? (char)*begin : (char)'.', (char)'\0'
                    };
                    stream << ch;
                }
                stream << std::endl;
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatBuffer (
                const void *buffer,
                std::size_t length) {
            std::ostringstream stream;
            if (buffer != 0 && length > 0) {
                const ui8 *ptr = (const ui8 *)buffer;
                const std::size_t charsPerRow = 16;
                std::size_t rows = length / charsPerRow;
                for (std::size_t i = 0; i < rows; ++i) {
                    HexFormatRow (stream, ptr, charsPerRow);
                    ASCIIFormatRow (stream, ptr, charsPerRow);
                    ptr += charsPerRow;
                }
                std::size_t remainder = length % charsPerRow;
                if (remainder > 0) {
                    HexFormatRow (stream, ptr, remainder);
                    stream << std::string ((charsPerRow - remainder) * 3, ' ');
                    ASCIIFormatRow (stream, ptr, remainder);
                }
            }
            return stream.str ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API HexFormatstring (
                const std::string &str) {
            return !str.empty () ? HexFormatBuffer (&str[0], str.size ()) : std::string ();
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API HashString (
                const std::string &str,
                std::size_t hashTableSize) {
            std::size_t hash = 5381;
            for (std::size_t i = 0, count = str.size (); i < count; ++i) {
                hash = ((hash << 5) + hash) ^ str[i]; // hash * 33 ^ str[i]
            }
            return hash % hashTableSize;
        }


        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetLongestCommonPrefix (
                const std::list<std::string> &strings) {
            std::string prefix;
            if (!strings.empty ()) {
                std::vector<std::string> vStrings;
                for (std::list<std::string>::const_iterator
                        it = strings.begin (),
                        end = strings.end (); it != end; ++it) {
                    vStrings.push_back (*it);
                }
                std::sort (vStrings.begin (), vStrings.end ());
                std::size_t index = 0;
                std::size_t lengthFirst = vStrings[0].size ();
                std::size_t lastIndex = vStrings.size () - 1;
                std::size_t lengthLast = vStrings[lastIndex].size ();
                for (; index < lengthFirst && index < lengthLast; ++index) {
                    if (vStrings[0][index] != vStrings[lastIndex][index]) {
                        break;
                    }
                }
                prefix = vStrings[0].substr (0, index);
            }
            return prefix;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatList (
                const std::list<std::string> &strings,
                const std::string &separator) {
            std::string formattedList;
            if (!strings.empty ()) {
                std::list<std::string>::const_iterator it = strings.begin ();
                formattedList = *it++;
                for (std::list<std::string>::const_iterator end = strings.end (); it != end; ++it) {
                    formattedList += separator + *it;
                }
            }
            return formattedList;
        }

        _LIB_THEKOGANS_UTIL_DECL std::size_t _LIB_THEKOGANS_UTIL_API stringTosize_t (
                const char *value,
                char **end,
                i32 base) {
        #if defined (TOOLCHAIN_ARCH_i386)
            return (std::size_t)strtoul (value, end, base);
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            return (std::size_t)strtoull (value, end, base);
        #else // defined (TOOLCHAIN_ARCH_i386)
            #error "Unknown architecture."
        #endif // defined (TOOLCHAIN_ARCH_i386)
        }

        _LIB_THEKOGANS_UTIL_DECL i16 _LIB_THEKOGANS_UTIL_API stringToi16 (
                const char *value,
                char **end,
                i32 base) {
            return (i16)strtol (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL ui16 _LIB_THEKOGANS_UTIL_API stringToui16 (
                const char *value,
                char **end,
                i32 base) {
            return (ui16)strtoul (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL i32 _LIB_THEKOGANS_UTIL_API stringToi32 (
                const char *value,
                char **end,
                i32 base) {
            return (i32)strtol (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL ui32 _LIB_THEKOGANS_UTIL_API stringToui32 (
                const char *value,
                char **end,
                i32 base) {
            return (ui32)strtoul (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL i64 _LIB_THEKOGANS_UTIL_API stringToi64 (
                const char *value,
                char **end,
                i32 base) {
            return (i64)strtoll (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL ui64 _LIB_THEKOGANS_UTIL_API stringToui64 (
                const char *value,
                char **end,
                i32 base) {
            return (ui64)strtoull (value, end, base);
        }

        _LIB_THEKOGANS_UTIL_DECL f32 _LIB_THEKOGANS_UTIL_API stringTof32 (
                const char *value,
                char **end) {
            return (f32)strtod (value, end);
        }

        _LIB_THEKOGANS_UTIL_DECL f64 _LIB_THEKOGANS_UTIL_API stringTouf64 (
                const char *value,
                char **end) {
            return (f64)strtod (value, end);
        }

        _LIB_THEKOGANS_UTIL_DECL time_t _LIB_THEKOGANS_UTIL_API stringTotime_t (
                const char *value,
                char **end) {
        #if defined (TOOLCHAIN_ARCH_i386)
            return (time_t)stringToi32 (value, end);
        #elif defined (TOOLCHAIN_ARCH_x86_64)
            return (time_t)stringToi64 (value, end);
        #else // defined (TOOLCHAIN_ARCH_i386)
            #error "Unknown architecture."
        #endif // defined (TOOLCHAIN_ARCH_i386)
        }

        _LIB_THEKOGANS_UTIL_DECL THEKOGANS_UTIL_ERROR_CODE _LIB_THEKOGANS_UTIL_API
        stringToTHEKOGANS_UTIL_ERROR_CODE (
                const char *value,
                char **end) {
        #if defined (TOOLCHAIN_OS_Windows)
            return (THEKOGANS_UTIL_ERROR_CODE)stringToui32 (value, end);
        #else // defined (TOOLCHAIN_OS_Windows)
            return (THEKOGANS_UTIL_ERROR_CODE)stringToi32 (value, end);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API size_tTostring (
                std::size_t value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API boolTostring (
                bool value) {
            return value ? XML_TRUE : XML_FALSE;
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i32Tostring (
                i32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui32Tostring (
                ui32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API i64Tostring (
                i64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ui64Tostring (
                ui64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f32Tostring (
                f32 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API f64Tostring (
                f64 value,
                const char *format) {
            return FormatString (format, value);
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API time_tTostring (
                time_t value,
                const char *format) {
            return FormatString (format, value);
        }

    #if !defined (TOOLCHAIN_OS_Windows)
        namespace {
            int _vscprintf (
                    const char *format,
                    va_list argptr) {
                va_list myargptr;
                va_copy (myargptr, argptr);
                int result = vsnprintf (0, 0, format, myargptr);
                va_end (myargptr);
                return result;
            }
        }
    #endif // !defined (TOOLCHAIN_OS_Windows)

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API FormatStringHelper (
                const char *format,
                va_list argptr) {
            if (format != 0) {
                std::string str;
                int size = _vscprintf (format, argptr);
                if (size > 0) {
                    str.resize (size + 1);
                    vsnprintf (&str[0], size + 1, format, argptr);
                    str.resize (size);
                }
                return str;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string FormatString (
                const char *format,
                ...) {
            if (format != 0) {
                va_list argptr;
                va_start (argptr, format);
                std::string str = FormatStringHelper (format, argptr);
                va_end (argptr);
                return str;
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API ErrorCodeTostring (
                THEKOGANS_UTIL_ERROR_CODE value) {
        #if defined (TOOLCHAIN_OS_Windows)
            return ui32Tostring (value);
        #else // defined (TOOLCHAIN_OS_Windows)
            return i32Tostring (value);
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

        _LIB_THEKOGANS_UTIL_DECL std::string _LIB_THEKOGANS_UTIL_API GetEnvironmentVariable (
                const char *name) {
        #if defined (TOOLCHAIN_OS_Windows)
            size_t requiredSize;
            getenv_s (&requiredSize, 0, 0, name);
            if (requiredSize > 0) {
                Array<char> value (requiredSize);
                if (getenv_s (&requiredSize, value, requiredSize, name) == 0) {
                    return value.array;
                }
                else {
                    THEKOGANS_UTIL_THROW_POSIX_ERROR_CODE_EXCEPTION (
                        THEKOGANS_UTIL_POSIX_OS_ERROR_CODE);
                }
            }
            return std::string ();
        #else // defined (TOOLCHAIN_OS_Windows)
            const char *value = name != 0 ? getenv (name) : 0;
            return value != 0 ? value : std::string ();
        #endif // defined (TOOLCHAIN_OS_Windows)
        }

    } // namespace util
} // namespace thekogans
