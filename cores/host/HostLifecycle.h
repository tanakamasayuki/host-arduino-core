#ifndef HOST_ARDUINO_LIFECYCLE_H
#define HOST_ARDUINO_LIFECYCLE_H

#include <stdint.h>

// The lifecycle port — four fixed points around the Arduino thunk.
//
// The thunk is `setup(); while (...) { loop(); runtimePoll(); }`, and the
// port announces the four boundaries a test driver needs:
//
//   kPreSetup    once, after the runtime is up and Serial works, before
//                setup() — where a driver arranges the fixture
//   kPostSetup   once, after setup() returns — where it dumps the state
//                the sketch left behind
//   kPreLoop     every iteration, before loop() — where a driver runs
//                whatever the sketch is about to observe: answers queued
//                on a bus, an ISR it wants delivered
//   kPostLoop    every iteration, after loop() returns — where it closes
//                the iteration out, typically by advancing a clock
//
// Order. `kPostLoop` runs before `runtimePoll()`, not after:
//
//   kPreLoop -> loop() -> kPostLoop -> runtimePoll()
//
// so external input is always taken in after the iteration has been
// closed out and is stamped as belonging to the *next* one. With the
// other order, input picked up by `runtimePoll()` would land in the
// iteration the sketch had already finished running, and a trace would
// show the sketch alongside bytes it never had a chance to see.
//
// One hook, one user. There is one slot and one phase callback rather
// than four separate slots, for the same reason the analog half of the
// bus port has one: the four points are an ordered sequence, and a driver
// that wants them as one stream should not have to stitch four streams
// back together. Multiplexing between several interested parties belongs
// to whatever layer sits above this one.
//
// Nothing is installed by default and an empty slot costs two calls per
// iteration through a null check, so a sketch that never registers
// behaves exactly as it did before this port existed.
//
// Both thunks announce the same four points: the plain host runtime and
// the SDL one used by `mode=lgfx` and the `display` board. Note that the
// SDL thunk runs on a worker thread, so a driver that registers from
// `main` must expect its hook on another thread there.

namespace HostArduino {

enum LifecyclePhase : uint8_t {
    kPreSetup = 0,
    kPostSetup,
    kPreLoop,
    kPostLoop,
};

using LifecycleHook = void (*)(LifecyclePhase phase, void *user);

// Passing `nullptr` unregisters. Registering replaces the previous hook.
void setLifecycleHook(LifecycleHook hook, void *user = nullptr);
void clearLifecycleHook();

// Iterations completed, counting a `kPostLoop` each. Useful as the tick
// number a driver stamps its own records with, and as a cheap assertion
// that the thunk ran the number of times a test expected.
uint64_t loopCount();

namespace lifecycle_detail {

// Called by the thunks in main.cpp. Core-internal.
void announce(LifecyclePhase phase);

} // namespace lifecycle_detail

} // namespace HostArduino

#endif
