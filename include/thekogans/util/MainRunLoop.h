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

#if !defined (__thekogans_util_MainRunLoop_h)
#define __thekogans_util_MainRunLoop_h

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
#elif defined (TOOLCHAIN_OS_OSX)
    #include <CoreFoundation/CFRunLoop.h>
#endif // defined (TOOLCHAIN_OS_Windows)
#include <memory>
#include <list>
#include "thekogans/util/Config.h"
#include "thekogans/util/SpinLock.h"
#include "thekogans/util/Singleton.h"
#include "thekogans/util/RunLoop.h"
#include "thekogans/util/SystemRunLoop.h"

namespace thekogans {
    namespace util {

        /// \struct MainRunLoopCreateInstance MainRunLoop.h thekogans/util/MainRunLoop.h
        ///
        /// \brief
        /// Call MainRunLoopCreateInstance::Parameterize before the first use of
        /// MainRunLoop::Instance to supply custom arguments to SystemRunLoop ctor.
        /// If you don't call MainRunLoopCreateInstance::Parameterize, MainRunLoop
        /// will create a \see{ThreadRunLoop} on it's first invocation of Instance.
        ///
        /// VERY IMPORTANT: MainRunLoopCreateInstance::Parameterize performs initialization
        /// (calls Thread::SetMainThread ()) that only makes sense when called from the
        /// main thread (main).
        ///
        /// Follow these templates in order to create the \see{SystemRunLoop} on the
        /// right thread:
        ///
        /// On Windows:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int CALLBACK WinMain (
        ///         HINSTANCE /*hInstance*/,
        ///         HINSTANCE /*hPrevInstance*/,
        ///         LPSTR /*lpCmdLine*/,
        ///         int /*nCmdShow*/) {
        ///     ...
        ///     util::MainRunLoopCreateInstance::Parameterize (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::Ptr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         0,
        ///         0,
        ///         util::SystemRunLoop::CreateThreadWindow ());
        ///     ...
        ///     BOOL result;
        ///     MSG msg;
        ///     while ((result = GetMessage (&msg, 0, 0, 0)) != 0) {
        ///         if (result != -1) {
        ///             TranslateMessage (&msg);
        ///             DispatchMessage (&msg);
        ///         }
        ///         else {
        ///             THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
        ///                 THEKOGANS_UTIL_OS_ERROR_CODE);
        ///         }
        ///     }
        ///     or
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// On Linux:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int main (
        ///         int /*argc*/,
        ///         const char * /*argv*/ []) {
        ///     ...
        ///     util::MainRunLoopCreateInstance::Parameterize (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::Ptr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         0,
        ///         0,
        ///         util::SystemRunLoop::CreateThreadWindow ());
        ///     ...
        ///     Display *display = XOpenDisplay (0);
        ///     while (1) {
        ///         XEvent event;
        ///         XNextEvent (display, &event);
        ///         if (!util::MainRunLoop::Instance ().DispatchEvent (display, event)) {
        ///             break;
        ///         }
        ///     }
        ///     or
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode
        ///
        /// On OS X:
        ///
        /// \code{.cpp}
        /// using namespace thekogans;
        ///
        /// int main (
        ///         int /*argc*/,
        ///         const char * /*argv*/ []) {
        ///     ...
        ///     // If using Cocoa, uncomment the following two lines.
        ///     //util::RunLoop::CocoaInitializer cocoaInitializer;
        ///     //util::RunLoop::WorkerInitializer workerInitializer (&cocoaInitializer);
        ///     ...
        ///     util::MainRunLoopCreateInstance::Parameterize (
        ///         "MainRunLoop",
        ///         util::RunLoop::JobExecutionPolicy::Ptr (
        ///             new util::RunLoop::FIFOJobExecutionPolicy),
        ///         SystemRunLoop::OSXRunLoop::Ptr (
        ///             new SystemRunLoop::CocoaOSXRunLoop
        ///             or
        ///             new SystemRunLoop::CFOSXRunLoop (CFRunLoopGetMain ())));
        ///     ...
        ///     util::MainRunLoop::Instance ().Start ();
        ///     ...
        ///     return 0;
        /// }
        /// \endcode

        struct _LIB_THEKOGANS_UTIL_DECL MainRunLoopCreateInstance {
            /// \brief
            /// "MainRunLoop"
            static const char * MAIN_RUN_LOOP_NAME;

        private:
            /// \brief
            /// \see{RunLoop} name.
            static std::string name;
            /// \brief
            /// JobQueue \see{RunLoop::JobExecutionPolicy}.
            static RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy;
            /// \brief
            /// true = the main thread will call MainRunLoop::Instance ().Start ().
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Callback to process Windows HWND events.
            static SystemRunLoop::EventProcessor eventProcessor;
            /// \brief
            /// Optional user data passed to eventProcessor.
            static void *userData;
            /// \brief
            /// Windows window.
            static Window::Ptr window;
        #elif defined (TOOLCHAIN_OS_Linux)
        #if defined (THEKOGANS_UTIL_HAVE_XLIB)
            /// \brief
            /// Callback to process Xlib XEvent events.
            static SystemRunLoop::EventProcessor eventProcessor;
            /// \brief
            /// Optional user data passed to eventProcessor.
            static void *userData;
            /// \brief
            /// Xlib server display name.
            static SystemRunLoop::XlibWindow::Ptr window;
            /// \brief
            /// A list of displays to listen to.
            static std::vector<Display *> displays;
        #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// OS X run loop object.
            static SystemRunLoop::OSXRunLoop::Ptr runLoop;
        #endif // defined (TOOLCHAIN_OS_Windows)

        public:
            /// \brief
            /// Call before the first use of MainRunLoop::Instance.
            /// \param[in] name_ RunLoop name.
            /// \param[in] jobExecutionPolicy_ JobQueue \see{RunLoop::JobExecutionPolicy}.
            static void Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_);
        #if defined (TOOLCHAIN_OS_Windows)
            /// \brief
            /// Call before the first use of MainRunLoop::Instance.
            /// \param[in] name_ RunLoop name.
            /// \param[in] jobExecutionPolicy_ JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] eventProcessor_ Callback to process Windows HWND events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Windows window.
            static void Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                Window::Ptr window_);
        #elif defined (TOOLCHAIN_OS_Linux)
        #if defined (THEKOGANS_UTIL_HAVE_XLIB)
            /// \brief
            /// Call before the first use of MainRunLoop::Instance.
            /// \param[in] name_ RunLoop name.
            /// \param[in] jobExecutionPolicy_ JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] eventProcessor_ Callback to process Xlib XEvent events.
            /// \param[in] userData_ Optional user data passed to eventProcessor.
            /// \param[in] window_ Xlib server window.
            /// \param[in] displays_ A list of displays to listen to.
            static void Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::EventProcessor eventProcessor_,
                void *userData_,
                SystemRunLoop::XlibWindow::Ptr window_,
                const std::vector<Display *> &displays_);
        #endif // defined (THEKOGANS_UTIL_HAVE_XLIB)
        #elif defined (TOOLCHAIN_OS_OSX)
            /// \brief
            /// Call before the first use of MainRunLoop::Instance.
            /// \param[in] name_ RunLoop name.
            /// \param[in] jobExecutionPolicy_ JobQueue \see{RunLoop::JobExecutionPolicy}.
            /// \param[in] runLoop_ OS X run loop object.
            static void Parameterize (
                const std::string &name_,
                RunLoop::JobExecutionPolicy::Ptr jobExecutionPolicy_,
                SystemRunLoop::OSXRunLoop::Ptr runLoop_);
        #endif // defined (TOOLCHAIN_OS_Windows)

            /// \brief
            /// Create a main thread run loop with custom ctor arguments.
            /// \return A main thread run loop with custom ctor arguments.
            RunLoop *operator () ();
        };

        /// \struct MainRunLoop MainRunLoop.h thekogans/util/MainRunLoop.h
        ///
        /// \brief
        /// Main thread run loop.
        struct _LIB_THEKOGANS_UTIL_DECL MainRunLoop :
            public Singleton<RunLoop, SpinLock, MainRunLoopCreateInstance> {};

    } // namespace util
} // namespace thekogans

#endif // !defined (__thekogans_util_MainRunLoop_h)
