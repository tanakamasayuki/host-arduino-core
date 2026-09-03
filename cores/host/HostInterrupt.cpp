#include "Arduino.h"

namespace HostArduino {
namespace {

// One slot per tracked pin, in a function-local static so a sketch's own
// global constructor can register before this translation unit's statics
// would have run.
struct InterruptTable {
    InterruptSlot slot[kGpioPinCount];

    InterruptTable() { reset(); }

    void reset()
    {
        for (int i = 0; i < kGpioPinCount; ++i) {
            slot[i] = InterruptSlot{};
            slot[i].pin = static_cast<uint8_t>(i);
        }
    }
};

InterruptTable &table()
{
    static InterruptTable t;
    return t;
}

InterruptHook interrupt_hook = nullptr;
void *interrupt_hook_user = nullptr;

bool inRange(int pin)
{
    return static_cast<unsigned>(pin) < static_cast<unsigned>(kGpioPinCount);
}

void report(InterruptEvent event, const InterruptSlot &slot)
{
    if (interrupt_hook) {
        interrupt_hook(event, slot, interrupt_hook_user);
    }
}

// The raw Arduino constant to a name that means the same thing on both
// targets. `LOW` (0) as a mode is the AVR spelling of level-low, which is
// why 0 is not treated as "no mode".
InterruptTrigger normalize(int mode)
{
    switch (mode) {
    case LOW:
        return kTriggerLevelLow;
    case CHANGE:
        return kTriggerChange;
    case FALLING:
        return kTriggerFalling;
    case RISING:
        return kTriggerRising;
    case ONLOW:
    case ONLOW_WE:
        return kTriggerLevelLow;
    case ONHIGH:
    case ONHIGH_WE:
        return kTriggerLevelHigh;
    default:
        return kTriggerUnknown;
    }
}

void attach(int pin, void (*handler)(void), void (*handler_arg)(void *), void *arg, int mode)
{
    if (!inRange(pin) || (!handler && !handler_arg)) {
        return;
    }
    InterruptSlot &slot = table().slot[pin];
    // Re-attaching replaces, which is what arduino-esp32 does. The fire
    // count survives so a driver can keep counting across a re-arm.
    slot.attached = true;
    slot.mode = mode;
    slot.trigger = normalize(mode);
    slot.handler = handler;
    slot.handler_arg = handler_arg;
    slot.arg = arg;
    report(kInterruptAttach, slot);
}

void detach(int pin)
{
    if (!inRange(pin)) {
        return;
    }
    InterruptSlot &slot = table().slot[pin];
    if (!slot.attached) {
        return; // arduino-esp32 logs and does nothing; nor do we
    }
    slot.attached = false;
    slot.mode = 0;
    slot.trigger = kTriggerNone;
    slot.handler = nullptr;
    slot.handler_arg = nullptr;
    slot.arg = nullptr;
    report(kInterruptDetach, slot);
}

} // namespace

void setInterruptHook(InterruptHook hook, void *user)
{
    interrupt_hook = hook;
    interrupt_hook_user = user;
}

void clearInterruptHook()
{
    interrupt_hook = nullptr;
    interrupt_hook_user = nullptr;
}

const InterruptSlot &interruptSlot(int pin)
{
    static const InterruptSlot none;
    return inRange(pin) ? table().slot[pin] : none;
}

bool interruptAttached(int pin)
{
    return inRange(pin) && table().slot[pin].attached;
}

InterruptTrigger interruptTrigger(int pin)
{
    return inRange(pin) ? table().slot[pin].trigger : kTriggerNone;
}

int interruptMode(int pin)
{
    return inRange(pin) ? table().slot[pin].mode : 0;
}

bool triggerInterrupt(int pin)
{
    if (!inRange(pin)) {
        return false;
    }
    InterruptSlot &slot = table().slot[pin];
    if (!slot.attached) {
        return false;
    }

    // Taken before the call: a handler is allowed to detach itself, or to
    // re-attach the pin to something else, and either would leave us
    // holding a pointer the slot no longer names.
    void (*handler)(void) = slot.handler;
    void (*handler_arg)(void *) = slot.handler_arg;
    void *arg = slot.arg;

    ++slot.fires;
    ++slot.depth;
    report(kInterruptEnter, slot);

    if (handler_arg) {
        handler_arg(arg);
    } else if (handler) {
        handler();
    }

    // The handler may have detached the pin, so `slot` is re-read rather
    // than cached. The depth is unwound whatever it did.
    if (slot.depth > 0) {
        --slot.depth;
    }
    report(kInterruptExit, slot);
    return true;
}

void resetInterrupts()
{
    table().reset();
}

} // namespace HostArduino

// --- Arduino surface -------------------------------------------------

void attachInterrupt(uint8_t pin, void (*handler)(void), int mode)
{
    HostArduino::attach(pin, handler, nullptr, nullptr, mode);
}

void attachInterruptArg(uint8_t pin, void (*handler)(void *), void *arg, int mode)
{
    HostArduino::attach(pin, nullptr, handler, arg, mode);
}

void detachInterrupt(uint8_t pin)
{
    HostArduino::detach(pin);
}
