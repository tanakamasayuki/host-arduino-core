// Tests for the interrupt port (cores/host/HostInterrupt.h).
//
// The split being verified: the core keeps what `attachInterrupt`
// registered and calls it when asked, and decides nothing. So the two
// halves of this sketch are
//
//   - an ordinary sketch that attaches ISRs and never thinks about how
//     they fire
//   - a driver that moves a line, decides for itself that the movement
//     matches the registration, and then calls the port
//
// The negative assertion matters as much as the positive one: moving the
// pin does *not* fire anything on its own. If the core inferred edges from
// `setPinValue` there would be two things deciding when an ISR runs.
//
// `kInterruptEnter` / `kInterruptExit` bracket the handler, so what an ISR
// puts on a bus is visible as being inside it. That is what the GPIO write
// hook is doing in the same trace here.

#include <Arduino.h>

#include <stdarg.h>

namespace {

constexpr int PIN_BUTTON = 27;
constexpr int PIN_LED = 2;
constexpr int PIN_SPARE = 33;

constexpr uint8_t kTraceMax = 32;

struct Trace {
    char line[kTraceMax][48] = {{0}};
    uint8_t count = 0;
    bool overflowed = false;

    void add(const char *format, ...) __attribute__((format(printf, 2, 3)))
    {
        if (count >= kTraceMax) {
            overflowed = true;
            return;
        }
        va_list args;
        va_start(args, format);
        vsnprintf(line[count], sizeof(line[0]), format, args);
        va_end(args);
        ++count;
    }
};

Trace trace;

const char *triggerName(HostArduino::InterruptTrigger trigger)
{
    switch (trigger) {
    case HostArduino::kTriggerNone:
        return "none";
    case HostArduino::kTriggerRising:
        return "rising";
    case HostArduino::kTriggerFalling:
        return "falling";
    case HostArduino::kTriggerChange:
        return "change";
    case HostArduino::kTriggerLevelLow:
        return "low";
    case HostArduino::kTriggerLevelHigh:
        return "high";
    case HostArduino::kTriggerUnknown:
        return "unknown";
    }
    return "?";
}

void onInterrupt(HostArduino::InterruptEvent event, const HostArduino::InterruptSlot &slot, void *user)
{
    Trace *t = static_cast<Trace *>(user);
    switch (event) {
    case HostArduino::kInterruptAttach:
        t->add("attach pin=%u mode=%d trig=%s arg=%d", slot.pin, slot.mode,
               triggerName(slot.trigger), slot.handler_arg ? 1 : 0);
        break;
    case HostArduino::kInterruptDetach:
        t->add("detach pin=%u", slot.pin);
        break;
    case HostArduino::kInterruptEnter:
        t->add("enter pin=%u depth=%u fires=%u", slot.pin, slot.depth, slot.fires);
        break;
    case HostArduino::kInterruptExit:
        t->add("exit pin=%u depth=%u", slot.pin, slot.depth);
        break;
    }
}

// In the same trace, so an ISR's own bus traffic shows up bracketed
// between enter and exit.
void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    static_cast<Trace *>(user)->add("gpio.write pin=%u value=%u", pin, value);
}

// --- the sketch's ISRs ------------------------------------------------

volatile int button_fires = 0;
volatile int spare_fires = 0;

void onButton()
{
    ++button_fires;
    // An ordinary ISR body. On the host there is no interrupt context, so
    // this is allowed to touch a pin — the trace shows it inside the
    // handler either way.
    digitalWrite(PIN_LED, HIGH);
}

struct Counter {
    int count = 0;
};

Counter counter;

void onSpareWithArg(void *arg)
{
    ++spare_fires;
    static_cast<Counter *>(arg)->count += 10;
}

// A handler that detaches itself, which the port has to survive: the
// pointer is taken before the call.
void onSelfDetach()
{
    detachInterrupt(PIN_SPARE);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start interrupt_hook");

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);

    // Nothing attached: the port refuses, and a sketch that never calls
    // attachInterrupt is in exactly the state it was before this port
    // existed.
    Serial.printf("unattached: attached=%d trig=%s fired=%d\n",
                  HostArduino::interruptAttached(PIN_BUTTON) ? 1 : 0,
                  triggerName(HostArduino::interruptTrigger(PIN_BUTTON)),
                  HostArduino::triggerInterrupt(PIN_BUTTON) ? 1 : 0);

    HostArduino::setInterruptHook(onInterrupt, &trace);

    // --- what a registration keeps -----------------------------------

    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButton, FALLING);
    const HostArduino::InterruptSlot &slot = HostArduino::interruptSlot(PIN_BUTTON);
    Serial.printf("attached: pin=%u mode=%d trig=%s handler=%d arg=%d\n", slot.pin, slot.mode,
                  triggerName(slot.trigger), slot.handler ? 1 : 0, slot.handler_arg ? 1 : 0);

    // The raw number is host-local — FALLING is 2 here and on
    // arduino-esp32, but RISING and CHANGE are swapped between them, which
    // is why `trigger` exists. A driver matches on that and never has to
    // know which core it is compiled against.
    Serial.printf("modes: change=%s falling=%s rising=%s low=%s high=%s\n",
                  triggerName(HostArduino::kTriggerChange),
                  triggerName(HostArduino::kTriggerFalling),
                  triggerName(HostArduino::kTriggerRising),
                  triggerName(HostArduino::kTriggerLevelLow),
                  triggerName(HostArduino::kTriggerLevelHigh));

    // --- moving the line fires nothing -------------------------------
    //
    // This is the whole division of labour. The core sees the write, holds
    // the level, announces it to the GPIO hook — and does not decide that
    // an ISR should run.
    HostArduino::setPinWriteHook(onPinWrite, &trace);
    const uint8_t before_move = trace.count;
    HostArduino::setPinValue(PIN_BUTTON, LOW);
    digitalWrite(PIN_BUTTON, LOW);
    digitalWrite(PIN_BUTTON, HIGH);
    Serial.printf("nofire: button_fires=%d slot_fires=%u trace_delta=%u\n", button_fires,
                  slot.fires, trace.count - before_move);

    // --- the driver decides, then calls ------------------------------
    //
    // What a driver does: it saw the line go high-to-low, it checked that
    // against the registered trigger, and only then did it call the port.
    HostArduino::setPinValue(PIN_BUTTON, LOW);
    const bool matches = HostArduino::interruptTrigger(PIN_BUTTON) == HostArduino::kTriggerFalling;
    const bool fired = matches && HostArduino::triggerInterrupt(PIN_BUTTON);
    Serial.printf("fired: matched=%d fired=%d button_fires=%d led=%d\n", matches ? 1 : 0,
                  fired ? 1 : 0, button_fires, HostArduino::pinValue(PIN_LED));

    // --- attachInterruptArg ------------------------------------------

    attachInterruptArg(PIN_SPARE, onSpareWithArg, &counter, RISING);
    HostArduino::triggerInterrupt(PIN_SPARE);
    HostArduino::triggerInterrupt(PIN_SPARE);
    Serial.printf("arg: spare_fires=%d count=%d slot_fires=%u\n", spare_fires, counter.count,
                  HostArduino::interruptSlot(PIN_SPARE).fires);

    // Re-attaching replaces the handler and keeps the fire count, so a
    // driver can keep counting across a re-arm.
    attachInterrupt(PIN_SPARE, onSelfDetach, CHANGE);
    const HostArduino::InterruptSlot &spare = HostArduino::interruptSlot(PIN_SPARE);
    Serial.printf("rearm: trig=%s handler=%d arg=%d fires=%u\n", triggerName(spare.trigger),
                  spare.handler ? 1 : 0, spare.handler_arg ? 1 : 0, spare.fires);

    // A handler that detaches itself mid-call is survivable, and the
    // second call then finds nothing.
    const bool detaching = HostArduino::triggerInterrupt(PIN_SPARE);
    const bool again = HostArduino::triggerInterrupt(PIN_SPARE);
    Serial.printf("selfdetach: first=%d second=%d attached=%d\n", detaching ? 1 : 0, again ? 1 : 0,
                  HostArduino::interruptAttached(PIN_SPARE) ? 1 : 0);

    // --- an unrecognized mode is reported, not guessed ---------------

    attachInterrupt(PIN_SPARE, onSelfDetach, 0x7F);
    Serial.printf("unknown: mode=%d trig=%s\n", HostArduino::interruptMode(PIN_SPARE),
                  triggerName(HostArduino::interruptTrigger(PIN_SPARE)));
    detachInterrupt(PIN_SPARE);

    // --- the recorded stream -----------------------------------------

    Serial.printf("trace=%u overflow=%d\n", trace.count, trace.overflowed ? 1 : 0);
    for (uint8_t i = 0; i < trace.count; ++i) {
        Serial.printf("| %s\n", trace.line[i]);
    }

    // Releasing the slots stops the notifications and leaves the
    // registration alone.
    HostArduino::clearInterruptHook();
    HostArduino::clearPinHooks();
    const uint8_t before_clear = trace.count;
    HostArduino::triggerInterrupt(PIN_BUTTON);
    Serial.printf("cleared: delta=%u button_fires=%d\n", trace.count - before_clear, button_fires);

    // And a reset detaches everything.
    HostArduino::resetInterrupts();
    Serial.printf("reset: attached=%d fires=%u fired=%d\n",
                  HostArduino::interruptAttached(PIN_BUTTON) ? 1 : 0,
                  HostArduino::interruptSlot(PIN_BUTTON).fires,
                  HostArduino::triggerInterrupt(PIN_BUTTON) ? 1 : 0);

    Serial.println("TEST done");
}

void loop()
{
    delay(10);
}
