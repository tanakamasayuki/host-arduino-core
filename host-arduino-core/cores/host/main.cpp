#include "Arduino.h"

#ifdef NATIVE_USE_SDL2

// LovyanGFX / M5GFX SDL backend.
//
// mode=lgfx and the display board both define NATIVE_USE_SDL2 (and link
// -lSDL2) while leaving ARDUINO undefined so M5GFX/LovyanGFX picks its SDL
// backend in device.hpp.
//
//   - lang-ship:host:host:mode=lgfx is CI-headless. It forces
//     SDL_VIDEODRIVER=dummy and keeps Serial-over-TCP via HostRuntime.
//   - lang-ship:host:display is foreground/manual. It leaves SDL's video
//     driver alone, skips the TCP runtime, and writes Serial to stdout.
//
// Panel_sdl::main drives the SDL event pump on the main thread and runs our
// setup()/loop() thunk on a worker thread. When the thunk exits, we push an
// SDL_QUIT event so Panel_sdl's main-thread event loop can release.
//
// Panel_sdl::main is forward-declared (not #included) because the core
// archive compiles before libraries; the symbol resolves at final link.

#include <stdint.h>
#include <stdlib.h>

#define SDL_MAIN_HANDLED
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#endif

namespace lgfx
{
    inline namespace v1
    {
        class Panel_sdl
        {
        public:
            static int main(int (*fn)(bool *), uint32_t msec_step_exec);
        };
    }
}

extern void setup();
extern void loop();

static int host_lgfx_thunk(bool *running)
{
    HostArduino::runtimeLogInfo("lgfx_thunk_enter");
    // The same four lifecycle points as the plain thunk below, in the
    // same order — see cores/host/HostLifecycle.h. This one runs on a
    // Panel_sdl worker thread, which is the only difference.
    HostArduino::lifecycle_detail::announce(HostArduino::kPreSetup);
    setup();
    HostArduino::lifecycle_detail::announce(HostArduino::kPostSetup);
    HostArduino::runtimeLogInfo("lgfx_setup_returned");
    while (*running && !HostArduino::runtimeShouldStop())
    {
        HostArduino::lifecycle_detail::announce(HostArduino::kPreLoop);
        loop();
        HostArduino::lifecycle_detail::announce(HostArduino::kPostLoop);
        HostArduino::runtimePoll();
    }
    // Release Panel_sdl::main's event loop. With SDL_VIDEODRIVER=dummy
    // no window-close / quit event arrives on its own, and Panel_sdl
    // only returns once every monitor is closing.
    SDL_Event quit_event{};
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
    return 0;
}

#ifdef _WIN32
int main(int argc, char **argv);
#else
int main(int argc, char **argv) __attribute__((weak));
#endif
int main(int argc, char **argv)
{
#ifndef HOST_ARDUINO_DISPLAY
#ifdef _WIN32
    _putenv("SDL_VIDEODRIVER=dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
#endif
    if (!HostArduino::runtimeStart(argc, argv))
    {
        return 1;
    }
    HostArduino::runtimeLogInfo("lgfx_panel_main_enter");
    const int rc = lgfx::v1::Panel_sdl::main(host_lgfx_thunk, 128);
    HostArduino::runtimeLogInfo("lgfx_panel_main_exit");
    HostArduino::runtimeStop();
    return rc;
}

#else // !NATIVE_USE_SDL2 — default Arduino host runtime

#ifdef _WIN32
int main(int argc, char **argv);
#else
int main(int argc, char **argv) __attribute__((weak));
#endif
int main(int argc, char **argv)
{
    if (!HostArduino::runtimeStart(argc, argv))
    {
        return 1;
    }

    // Lifecycle points, see cores/host/HostLifecycle.h. `kPostLoop` runs
    // before `runtimePoll()` so external input is always taken in after
    // the iteration has been closed out.
    HostArduino::lifecycle_detail::announce(HostArduino::kPreSetup);
    setup();
    HostArduino::lifecycle_detail::announce(HostArduino::kPostSetup);
    while (!HostArduino::runtimeShouldStop())
    {
        HostArduino::lifecycle_detail::announce(HostArduino::kPreLoop);
        loop();
        HostArduino::lifecycle_detail::announce(HostArduino::kPostLoop);
        HostArduino::runtimePoll();
    }

    HostArduino::runtimeStop();
    return 0;
}

#endif
