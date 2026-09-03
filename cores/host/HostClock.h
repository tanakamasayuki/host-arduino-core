#ifndef HOST_ARDUINO_CLOCK_H
#define HOST_ARDUINO_CLOCK_H

#include <stdint.h>

// The clock port — one place the host asks what time it is and one place
// it waits.
//
// Every sketch-facing time function goes through these two: `millis`,
// `micros`, `delay`, `delayMicroseconds`, `yield`, and the `Stream`
// timeouts that `readBytes` / `find` / `parseInt` are built on. Override
// them and the sketch's whole notion of time moves with it.
//
// Why the wait and not just the clock. A test driver that only replaced
// "what time is it" would still sit through every `delay(1000)` in real
// seconds, because the waiting happens somewhere else. Handing over the
// wait as well is what lets a driver either (a) advance a virtual clock
// and return at once, so a sketch full of `delay` runs at host speed, or
// (b) keep sleeping for real but do its own work in the gaps. Those are
// the same mechanism, not two features: the override *is* the tick
// callback.
//
// What stays in the core. `delay`'s loop, `runtimePoll()`, and the
// shutdown check are core code and are not overridable, so a driver
// cannot forget them:
//
//   deadline = clockNowMicros() + ms * 1000
//   while (!runtimeShouldStop() && clockNowMicros() < deadline) {
//       runtimePoll();
//       clockWaitMicros(1000);
//   }
//
// Overriding both hooks together is the normal case. Overriding only the
// wait keeps real time and gives you the 1 ms heartbeat. Overriding only
// `now` is the one combination to avoid: the deadline would be virtual
// while the wait stayed real, so the loop above would spin without the
// clock ever reaching the deadline.
//
// This port does not cover every wait in the core. The internal timeouts
// that stay on real time are listed under "Timeouts that stay on real
// time" in README.md — a test written against a virtual clock needs to
// know which readings will not agree with it.
//
// Threading. The hooks are called on whichever thread called into them,
// which includes FreeRTOS task threads (`std::thread` here). An override
// that keeps state must expect that; the default implementation is
// thread-safe because it holds none.

namespace HostArduino {

// Microseconds since the first time the clock was read — so the first
// reading a process makes is near zero, and `millis()` starts from there
// rather than from some arbitrary boot instant. `millis` and `micros` are
// this value truncated to 32 bits, so they wrap the way they do on real
// silicon. An override is expected to be monotonic; it is never asked to
// go backwards.
using ClockNowHook = uint64_t (*)(void *user);

// Wait roughly `micros` microseconds. Called with 0 from `yield()`, which
// is not a timed wait but is the one place a busy-waiting sketch offers
// the host a chance to run. The default treats 0 as a thread yield.
using ClockWaitHook = void (*)(uint32_t micros, void *user);

// Install both. Either may be `nullptr` to keep the real-time default for
// that half; passing `nullptr` for both is the same as `clearClockHooks`.
// One slot per kind, as everywhere else in this core — multiplexing
// belongs to whatever layer sits above.
void setClockHooks(ClockNowHook now, ClockWaitHook wait, void *user = nullptr);
void clearClockHooks();

// True while either hook is installed. A test can assert it is running on
// a virtual clock rather than guessing from timings.
bool clockOverridden();

uint64_t clockNowMicros();
void clockWaitMicros(uint32_t micros);

// The real monotonic clock and the real sleep, whatever is installed.
// This is what the core's own startup and socket paths use — see the
// README section named above for why those must not be virtualized.
uint64_t clockRealNowMicros();
void clockRealWaitMicros(uint32_t micros);

} // namespace HostArduino

#endif
