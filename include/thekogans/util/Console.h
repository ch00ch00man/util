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

#if !defined (__thekogans_util_Console_h)
#define __thekogans_util_Console_h

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
#endif // defined (TOOLCHAIN_OS_Windows)
#include <string>
#include "thekogans/util/Config.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Event.h"
#include "thekogans/util/JobQueue.h"
#include "thekogans/util/TimeSpec.h"

namespace thekogans {
    namespace util {

        struct Console;

        /// \struct ConsoleCreateInstance Console.h thekogans/util/Console.h
        ///
        /// \brief
        /// Call ConsoleCreateInstance::Parameterize before the first use of
        /// Conolse::Instance to supply custom arguments to Console ctor.

        struct _LIB_THEKOGANS_UTIL_DECL ConsoleCreateInstance {
        private:
            /// \brief
            /// true == Serialize access to std::cout and std::cerr.
            static bool threadSafePrintString;
            /// \brief
            /// true == Hook CTRL-C to call MainRunLoop::Instance ().Stop ().
            static bool hookCtrlBreak;
            /// \brief
            /// Linux and OS X only. true == Hook SIGCHLD to call wait.
            /// IMPORTANT: hookChild allows you to reap zombie children
            /// without waiting for them explicitly. If you use this
            /// functionality you need to use \see{ChildProcess::Spawn}
            /// as \see{ChildProcess::Exec} call wait internally.
            static bool hookChild;
            /// \brief
            /// Linux only. true == Turn on core dump.
            static bool coreDump;

        public:
            /// \brief
            /// Call before the first use of Console::Instance.
            /// \param[in] threadSafePrintString_ true == Serialize access to std::cout and std::cerr.
            /// \param[in] hookCtrlBreak_ true == Hook CTRL-C to call MainRunLoop::Instance ().Stop ().
            /// \param[in] hookChild_ Linux and OS X only. true == Hook SIGCHLD to avoid zombie children.
            /// NOTE: You should only pass in true for hookChild_ if you're calling \see{ChildProcess::Spawn}
            /// (instead of \see{ChildProcess::Exec}, and you don't want to reap the zombie children yourself.
            /// \param[in] coreDump_ Linux only. true == Turn on core dump.
            static void Parameterize (
                bool threadSafePrintString_ = true,
                bool hookCtrlBreak_ = true,
                bool hookChild_ = false,
                bool coreDump_ = true);

            /// \brief
            /// Create a console with custom ctor arguments.
            /// \return A console with custom ctor arguments.
            Console *operator () ();
        };

        /// \struct Console Console.h thekogans/util/Console.h
        ///
        /// \brief
        /// Console provides platform independent CTRL+BREAK handling and
        /// colored text output. On Linux and OS X it also turns on core
        /// dumping, and ignores the SIGPIPE.

        struct _LIB_THEKOGANS_UTIL_DECL Console :
                public Singleton<Console, SpinLock, ConsoleCreateInstance> {
        private:
            /// \brief
            /// Used to synchronize access to std::cout and std::cerr in PrintString.
            JobQueue::Ptr jobQueue;

        public:
            /// \brief
            /// ctor.
            /// \param[in] threadSafePrintString true == Serialize access to std::cout and std::cerr.
            /// \param[in] hookCtrlBreak true == Hook CTRL-C to call MainRunLoop::Instance ().Stop ().
            /// \param[in] hookChild Linux and OS X only. true == Hook SIGCHLD to avoid zombie children.
            /// NOTE: You should only pass in true for hookChild_ if you're calling \see{ChildProcess::Spawn}
            /// (instead of \see{ChildProcess::Exec}, and you don't want to reap the zombie children yourself.
            /// \param[in] coreDump Linux only. true == Turn on core dump.
            Console (
                bool threadSafePrintString = true,
                bool hookCtrlBreak = true,
                bool hookChild = false,
                bool coreDump = true);

            enum {
                /// \brief
                /// Print to std::cout.
                PRINT_COUT,
                /// \brief
                /// Print to std::cerr.
                PRINT_CERR
            };

        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Color type.
            typedef WORD ColorType;
        #else // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Color type.
            typedef char *ColorType;
        #endif // defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Red.
            static const ColorType TEXT_COLOR_RED;
            /// \brief
            /// Greeen.
            static const ColorType TEXT_COLOR_GREEN;
            /// \brief
            /// Yellow.
            static const ColorType TEXT_COLOR_YELLOW;
            /// \brief
            /// Blue.
            static const ColorType TEXT_COLOR_BLUE;
            /// \brief
            /// Magenta.
            static const ColorType TEXT_COLOR_MAGENTA;
            /// \brief
            /// Cyan.
            static const ColorType TEXT_COLOR_CYAN;
            /// \brief
            /// White.
            static const ColorType TEXT_COLOR_WHITE;

            /// \brief
            /// Print a string to std::cout or std::cerr.
            /// \param[in] str String to print.
            /// \param[in] where PRINT_COUT or PRINT_CERR.
            /// \param[in] color Text color.
            void PrintString (
                const std::string &str,
                ui32 where = PRINT_COUT,
                const ColorType color = 0);

            /// \brief
            /// if threadSafePrintString == true, wait for jobQueue to become idle.
            ///
            /// VERY IMPORTANT: If threadSafePrintString == true, there exists a race
            /// between application close and the print jobQueue flushing it's queue.
            /// If you use \see{LoggerMgr} and setup your main properly:
            ///
            /// \code{.cpp}
            /// int main (
            ///         int argc,
            ///         const char *argv[]) {
            ///     THEKOGANS_UTIL_TRY {
            ///         THEKOGANS_UTIL_LOG_INIT (argv[0]);
            ///         THEKOGANS_UTIL_LOG_RESET_EX (...);
            ///         THEKOGANS_UTIL_LOG_ADD_LOGGER (
            ///             thekogans::util::Logger::Ptr (new thekogans::util::ConsoleLogger));
            ///         ...
            ///         thekogans::util::MainRunLoop::Instance ().Start ();
            ///         THEKOGANS_UTIL_LOG_DEBUG ("%s exiting.\n", argv[0]);
            ///     }
            ///     THEKOGANS_UTIL_CATCH_AND_LOG
            ///     THEKOGANS_UTIL_LOG_FLUSH
            ///     return 0;
            /// }
            /// \endcode
            ///
            /// there's nothing for you to do as THEKOGANS_UTIL_LOG_FLUSH will take care
            /// of calling thekogans::util::Console::Instance ().FlushPrintQueue ().
            ///
            /// If you do something else, you need to make sure to call FlushPrintQueue
            /// yourself or you risk having your application deadlock on exit.
            /// \param[in] timeSpec How long to wait for FlushPrintQueue to complete.
            /// IMPORTANT: timeSpec is a relative value.
            void FlushPrintQueue (const TimeSpec &timeSpec = TimeSpec::Infinite);

            /// \brief
            /// Console is neither copy constructable, nor assignable.
            THEKOGANS_UTIL_DISALLOW_COPY_AND_ASSIGN (Console)
        };

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_Console_h)
