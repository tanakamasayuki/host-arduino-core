#ifndef HOST_ARDUINO_INTERRUPT_H
#define HOST_ARDUINO_INTERRUPT_H

#include <stdint.h>

#include "HostBus.h" // kGpioPinCount

// The interrupt port — registration kept, invocation offered, nothing
// decided.
//
// `attachInterrupt` used to be a no-op here, so an ISR-driven sketch
// linked and then simply never fired. This port keeps what the sketch
// registered and lets external code call it:
//
//   sketch  --attachInterrupt(pin, isr, RISING)--> core keeps pin/mode/isr
//   driver  --triggerInterrupt(pin)-------------> core calls isr
//
// What the core deliberately does NOT do:
//
//   - watch pin levels. `setPinValue` and `digitalWrite` do not produce
//     interrupts. Noticing that a line moved is the driver's job.
//   - compare the movement against the registered mode. `interruptTrigger`
//     is there so the driver can do that comparison itself.
//   - queue, coalesce, or refuse nested invocations. A handler that
//     triggers another one recurses, and how deep is the driver's policy.
//
// That split exists so one place owns the ordering of value change, log
// entry, edge decision, and handler call. A core that inferred edges from
// `setPinValue` would be a second, competing decision-maker.
//
// Modes. `interruptTrigger()` and `InterruptSlot::trigger` report a
// normalized enum rather than only the raw Arduino constant, because the
// raw numbers do not agree with arduino-esp32: `RISING` is 3 here and 1
// there, `CHANGE` is 1 here and 3 there. A driver that matched on the
// number would not get a mismatch, it would get a silent *wrong* match.
// Matching on `kTriggerRising` cannot go wrong that way. The raw value the
// sketch passed stays available as `InterruptSlot::mode` for a driver that
// wants to assert the literal call.
//
// Threading. The handler runs synchronously on whichever thread called
// `triggerInterrupt`, with no locking around the slot — the same terms as
// the rest of the observation port. There is no ISR context here: a
// handler can call anything a sketch can call, `Serial.print` included,
// which is not true on silicon.

namespace HostArduino {

// What the registered mode means, independent of the numbering. Levels
// collapse `ONLOW_WE` / `ONHIGH_WE` onto `ONLOW` / `ONHIGH`, since the
// wake-enable half describes sleep behavior this core does not have.
enum InterruptTrigger : uint8_t {
    kTriggerNone = 0, // nothing attached
    kTriggerRising,
    kTriggerFalling,
    kTriggerChange,
    kTriggerLevelLow,
    kTriggerLevelHigh,
    kTriggerUnknown, // attached with a mode this core does not recognize
};

enum InterruptEvent : uint8_t {
    kInterruptAttach = 0,
    kInterruptDetach,
    // Around the handler, so whatever it puts on a bus is bracketed in a
    // trace and nesting is visible. `slot.depth` is the depth including
    // this invocation.
    kInterruptEnter,
    kInterruptExit,
};

// Everything the core knows about one pin's registration, handed to the
// hook and returned by `interruptSlot`.
struct InterruptSlot {
    uint8_t pin = 0;
    bool attached = false;
    // The raw Arduino constant as the sketch passed it. Host-local: see
    // the note about numbering above.
    int mode = 0;
    InterruptTrigger trigger = kTriggerNone;
    // Exactly one of these is set while attached, depending on which
    // spelling registered it.
    void (*handler)(void) = nullptr;
    void (*handler_arg)(void *) = nullptr;
    void *arg = nullptr;
    // Handler invocations since the last reset, whether or not a hook was
    // watching.
    uint32_t fires = 0;
    // Nesting depth right now: 0 outside the handler, 1 inside it, more if
    // it triggered itself again.
    uint8_t depth = 0;
};

using InterruptHook = void (*)(InterruptEvent event, const InterruptSlot &slot, void *user);

// One slot, `nullptr` unregisters, registering replaces — as everywhere
// else in this core. Multiplexing belongs to the layer above.
void setInterruptHook(InterruptHook hook, void *user = nullptr);
void clearInterruptHook();

// Registration state of `pin`. Out-of-range pins report a detached slot.
const InterruptSlot &interruptSlot(int pin);
bool interruptAttached(int pin);
InterruptTrigger interruptTrigger(int pin);
int interruptMode(int pin);

// Call the handler registered for `pin`, synchronously, on this thread.
// Returns false when nothing is attached — the pin level and the
// registered mode are not consulted, so a caller that has decided an edge
// happened and matches is the only thing standing between a line moving
// and a handler running.
//
// A handler may detach itself: the pointer is taken before the call.
bool triggerInterrupt(int pin);

// Detach every pin and zero the counters, without touching the hook.
void resetInterrupts();

} // namespace HostArduino

#endif
