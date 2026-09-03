#include "HostClock.h"

#include <chrono>
#include <thread>

namespace HostArduino {
namespace {

// The epoch is taken on the first read, so `millis()` starts near zero for
// the sketch rather than counting from some arbitrary boot instant. Held
// in a function-local static so a sketch's own global constructor can read
// the clock before this translation unit's statics would have run.
std::chrono::steady_clock::time_point &epoch()
{
    static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    return t0;
}

ClockNowHook now_hook = nullptr;
ClockWaitHook wait_hook = nullptr;
void *hook_user = nullptr;

} // namespace

void setClockHooks(ClockNowHook now, ClockWaitHook wait, void *user)
{
    // Seed the epoch before handing the clock over, so the real clock and
    // an override that later hands back agree on where zero was.
    (void)epoch();
    now_hook = now;
    wait_hook = wait;
    hook_user = user;
}

void clearClockHooks()
{
    now_hook = nullptr;
    wait_hook = nullptr;
    hook_user = nullptr;
}

bool clockOverridden()
{
    return now_hook != nullptr || wait_hook != nullptr;
}

uint64_t clockNowMicros()
{
    if (now_hook) {
        return now_hook(hook_user);
    }
    return clockRealNowMicros();
}

void clockWaitMicros(uint32_t micros)
{
    if (wait_hook) {
        wait_hook(micros, hook_user);
        return;
    }
    clockRealWaitMicros(micros);
}

uint64_t clockRealNowMicros()
{
    const auto delta = std::chrono::steady_clock::now() - epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
}

void clockRealWaitMicros(uint32_t micros)
{
    if (micros == 0) {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

} // namespace HostArduino
