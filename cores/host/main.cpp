#include "Arduino.h"

#ifdef NATIVE_USE_SDL2

// LovyanGFX / M5GFX headless backend.
//
// The mode=lgfx menu defines NATIVE_USE_SDL2 (and links -lSDL2) but
// leaves ARDUINO undefined so M5GFX/LovyanGFX picks its SDL backend in
// device.hpp. We:
//   1. Force SDL_VIDEODRIVER=dummy before any SDL_Init runs, so no
//      window appears (CI-headless).
//   2. Bring up the host Arduino runtime (Serial-over-TCP via
//      HostRuntime) so the sketch's Serial.print() reaches the
//      pytest-embedded dut fixture.
//   3. Hand off to lgfx::v1::Panel_sdl::main which drives the SDL
//      event pump on the main thread and runs our setup()/loop()
//      thunk on a worker thread.
//   4. When the thunk exits (runtimeShouldStop becomes true after the
//      pytest dut disconnects), push an SDL_QUIT event so Panel_sdl's
//      main-thread event loop can release. Without that nudge the
//      dummy video driver never delivers a window-close / quit event
//      on its own and Panel_sdl::main blocks forever.
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
    setup();
    while (*running && !HostArduino::runtimeShouldStop())
    {
        loop();
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
#ifdef _WIN32
    _putenv("SDL_VIDEODRIVER=dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    if (!HostArduino::runtimeStart(argc, argv))
    {
        return 1;
    }
    const int rc = lgfx::v1::Panel_sdl::main(host_lgfx_thunk, 128);
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

    setup();
    while (!HostArduino::runtimeShouldStop())
    {
        loop();
        HostArduino::runtimePoll();
    }

    HostArduino::runtimeStop();
    return 0;
}

#endif
