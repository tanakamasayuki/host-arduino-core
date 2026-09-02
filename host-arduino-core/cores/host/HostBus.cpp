#include "Arduino.h"
#include "HostDiag.h"

#include <string.h>

namespace HostArduino {
namespace bus_detail {

uint8_t pin_value[kGpioPinCount] = {0};
uint8_t pin_mode[kGpioPinCount] = {0};
PinWriteHook pin_write_hook = nullptr;
void *pin_write_hook_user = nullptr;
PinReadHook pin_read_hook = nullptr;
void *pin_read_hook_user = nullptr;
PinModeHook pin_mode_hook = nullptr;
void *pin_mode_hook_user = nullptr;

static inline bool inRange(int pin)
{
    return static_cast<unsigned>(pin) < static_cast<unsigned>(kGpioPinCount);
}

void applyPinMode(int pin, int mode)
{
    if (!inRange(pin)) {
        return;
    }
    pin_mode[pin] = static_cast<uint8_t>(mode);

    // On real silicon a pulled-up input reads HIGH while nothing drives
    // the line, and a pulled-down one reads LOW. Seeding the stored value
    // that way is what makes a released open-drain line read back
    // correctly — soft I2C letting SDA go, and then reading the idle bus.
    //
    // `INPUT` (and the output modes) are left alone: a floating pin has
    // no defined level, so we keep whatever was last written and let the
    // sketch or a device model decide what the line is doing.
    if (mode == INPUT_PULLUP) {
        pin_value[pin] = 1;
    } else if (mode == INPUT_PULLDOWN) {
        pin_value[pin] = 0;
    }

    if (pin_mode_hook) {
        pin_mode_hook(static_cast<uint8_t>(pin), static_cast<uint8_t>(mode), pin_mode_hook_user);
    }
}

} // namespace bus_detail

void setPinWriteHook(PinWriteHook hook, void *user)
{
    bus_detail::pin_write_hook = hook;
    bus_detail::pin_write_hook_user = user;
}

void setPinModeHook(PinModeHook hook, void *user)
{
    bus_detail::pin_mode_hook = hook;
    bus_detail::pin_mode_hook_user = user;
}

void setPinReadHook(PinReadHook hook, void *user)
{
    bus_detail::pin_read_hook = hook;
    bus_detail::pin_read_hook_user = user;
}

void clearPinHooks()
{
    bus_detail::pin_write_hook = nullptr;
    bus_detail::pin_write_hook_user = nullptr;
    bus_detail::pin_read_hook = nullptr;
    bus_detail::pin_read_hook_user = nullptr;
    bus_detail::pin_mode_hook = nullptr;
    bus_detail::pin_mode_hook_user = nullptr;
}

uint8_t pinValue(int pin)
{
    return bus_detail::inRange(pin) ? bus_detail::pin_value[pin] : 0;
}

void setPinValue(int pin, uint8_t value)
{
    if (bus_detail::inRange(pin)) {
        bus_detail::pin_value[pin] = value ? 1 : 0;
    }
}

uint8_t pinModeOf(int pin)
{
    return bus_detail::inRange(pin) ? bus_detail::pin_mode[pin] : 0;
}

void resetPinState()
{
    memset(bus_detail::pin_value, 0, sizeof(bus_detail::pin_value));
    memset(bus_detail::pin_mode, 0, sizeof(bus_detail::pin_mode));
}

} // namespace HostArduino

// --- Analog / PWM half -----------------------------------------------
//
// The Arduino-facing spellings (`analogWrite`, `ledc*`, `dacWrite`,
// `tone`) are the global functions at the bottom of this file; they are
// declared in HostRuntime.h and deliberately not inline. Unlike
// `digitalWrite` there is no million-calls-per-frame path here, so
// keeping them out of every translation unit is worth more than saving an
// indirect call.

namespace {

// Held in one function-local static rather than as namespace-scope
// objects so a sketch's own global constructor can call `analogWrite`
// before this translation unit's statics would have run.
struct AnalogTables {
    HostArduino::AnalogOut out[HostArduino::kGpioPinCount];
    uint16_t value[HostArduino::kGpioPinCount];
    uint32_t millivolts[HostArduino::kGpioPinCount];
    // arduino-esp32 defaults for `analogWrite`'s implicit attach.
    uint8_t read_bits;
    uint8_t write_bits;
    uint32_t write_hz;
    // arduino-esp32 drives one tone at a time and refuses a second pin
    // until `noTone`; -1 when nothing is playing.
    int tone_pin;

    AnalogTables() { reset(); }

    void reset()
    {
        for (int i = 0; i < HostArduino::kGpioPinCount; ++i) {
            out[i] = HostArduino::AnalogOut{};
            out[i].pin = static_cast<uint8_t>(i);
            value[i] = 0;
            millivolts[i] = 0;
        }
        read_bits = 12;
        write_bits = 8;
        write_hz = 1000;
        tone_pin = -1;
    }
};

AnalogTables &tables()
{
    static AnalogTables t;
    return t;
}

HostArduino::AnalogWriteHook analog_write_hook = nullptr;
void *analog_write_hook_user = nullptr;
HostArduino::AnalogReadHook analog_read_hook = nullptr;
void *analog_read_hook_user = nullptr;

bool analogInRange(int pin)
{
    return static_cast<unsigned>(pin) < static_cast<unsigned>(HostArduino::kGpioPinCount);
}

void report(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out)
{
    if (analog_write_hook) {
        analog_write_hook(event, out, analog_write_hook_user);
    }
}

// The pin's LEDC slot, or nullptr when the pin is not attached to LEDC —
// a DAC pin included, since it has no channel behind it. Every `ledc*`
// entry point resolves through this so all of them refuse exactly the
// cases silicon refuses.
HostArduino::AnalogOut *ledcSlot(int pin)
{
    if (!analogInRange(pin)) {
        return nullptr;
    }
    HostArduino::AnalogOut &slot = tables().out[pin];
    return (slot.attached && !slot.dac) ? &slot : nullptr;
}

// Which pin holds a channel. Derived from the per-pin slots rather than
// kept in a reverse table, so the two can never disagree — and because
// several pins may share one channel, which a single reverse entry could
// not express. Returns the lowest such pin, or -1 when the channel is
// free. The scan is 256 entries on a cold path; the alternative was a
// per-channel pin list.
int channelOwner(int channel)
{
    if (static_cast<unsigned>(channel) >= static_cast<unsigned>(HostArduino::kLedcChannelCount)) {
        return -1;
    }
    for (int pin = 0; pin < HostArduino::kGpioPinCount; ++pin) {
        const HostArduino::AnalogOut *slot = ledcSlot(pin);
        if (slot && slot->channel == channel) {
            return pin;
        }
    }
    return -1;
}

// LEDC "full on" fixup, copied from arduino-esp32: a duty at or above the
// maximum is bumped one past it so the output stays high for the whole
// period. The `max_duty != 1` guard keeps 1-bit resolution alone, same as
// upstream.
uint32_t clampDuty(uint32_t duty, uint8_t resolution)
{
    const uint32_t max_duty = (1UL << resolution) - 1UL;
    if (duty >= max_duty && max_duty != 1) {
        return max_duty + 1;
    }
    return duty;
}

// Attach `pin` to `channel`. Mirrors ledcAttachChannel's refusals: an
// out-of-range channel, a zero frequency, a resolution of zero or wider
// than the timer, and a pin already attached to LEDC. When the channel is
// already carrying another pin, the frequency and resolution arguments are
// ignored and the channel's own settings win — arduino-esp32 logs that
// and does the same.
bool attachChannel(int pin, uint32_t freq, uint8_t resolution, uint8_t channel)
{
    if (!analogInRange(pin) || channel >= HostArduino::kLedcChannelCount) {
        return false;
    }
    if (freq == 0 || resolution == 0 || resolution > HostArduino::kLedcMaxResolution) {
        return false;
    }
    AnalogTables &t = tables();
    // A pin last written through `dacWrite` is repurposed rather than
    // refused, which is what arduino-esp32's `perimanClearPinBus` does.
    // No detach event for it: the attach below carries the pin's new
    // state, and an extra line would put an event in the trace that
    // silicon does not produce.
    if (ledcSlot(pin)) {
        return false;
    }

    const int sharing = channelOwner(channel);
    if (sharing >= 0) {
        freq = t.out[sharing].frequency;
        resolution = t.out[sharing].resolution;
    }

    HostArduino::AnalogOut &slot = t.out[pin];
    slot.channel = channel;
    slot.frequency = freq;
    slot.resolution = resolution;
    slot.attached = true;
    slot.dac = false;
    report(HostArduino::kAnalogAttach, slot);
    return true;
}

// Lowest free channel, or -1. arduino-esp32 picks the same one, so a
// trace shows the channel the sketch would have been given on silicon.
int firstFreeChannel()
{
    for (int i = 0; i < HostArduino::kLedcChannelCount; ++i) {
        if (channelOwner(i) < 0) {
            return i;
        }
    }
    return -1;
}

bool writeDuty(int pin, uint32_t duty)
{
    HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot) {
        return false;
    }
    slot->duty = clampDuty(duty, slot->resolution);
    report(HostArduino::kAnalogWrite, *slot);
    return true;
}

} // namespace

namespace HostArduino {

void setAnalogWriteHook(AnalogWriteHook hook, void *user)
{
    analog_write_hook = hook;
    analog_write_hook_user = user;
}

void setAnalogReadHook(AnalogReadHook hook, void *user)
{
    analog_read_hook = hook;
    analog_read_hook_user = user;
}

void clearAnalogHooks()
{
    analog_write_hook = nullptr;
    analog_write_hook_user = nullptr;
    analog_read_hook = nullptr;
    analog_read_hook_user = nullptr;
}

const AnalogOut &analogOut(int pin)
{
    static const AnalogOut none;
    return analogInRange(pin) ? tables().out[pin] : none;
}

int ledcChannelPin(int channel)
{
    return channelOwner(channel);
}

void setAnalogValue(int pin, uint16_t value)
{
    if (analogInRange(pin)) {
        tables().value[pin] = value;
    }
}

uint16_t analogValue(int pin)
{
    return analogInRange(pin) ? tables().value[pin] : 0;
}

void setAnalogMilliVolts(int pin, uint32_t millivolts)
{
    if (analogInRange(pin)) {
        tables().millivolts[pin] = millivolts;
    }
}

uint32_t analogMilliVolts(int pin)
{
    return analogInRange(pin) ? tables().millivolts[pin] : 0;
}

uint8_t analogReadBits()
{
    return tables().read_bits;
}

uint8_t analogWriteBits()
{
    return tables().write_bits;
}

uint32_t analogWriteHz()
{
    return tables().write_hz;
}

void resetAnalogState()
{
    tables().reset();
}

} // namespace HostArduino

// --- Analog input (ADC) ----------------------------------------------

uint16_t analogRead(uint8_t pin)
{
    const uint16_t held = tables().value[pin];
    if (analog_read_hook) {
        return analog_read_hook(pin, held, analog_read_hook_user);
    }
    return held;
}

uint32_t analogReadMilliVolts(uint8_t pin)
{
    return tables().millivolts[pin];
}

void analogReadResolution(uint8_t bits)
{
    tables().read_bits = bits;
}

void analogSetWidth(uint8_t bits)
{
    // Legacy spelling of the same knob on arduino-esp32.
    tables().read_bits = bits;
}

// --- Analog output: the LEDC family ----------------------------------

bool ledcAttachChannel(uint8_t pin, uint32_t freq, uint8_t resolution, uint8_t channel)
{
    return attachChannel(pin, freq, resolution, channel);
}

bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution)
{
    const int channel = firstFreeChannel();
    if (channel < 0) {
        return false;
    }
    return attachChannel(pin, freq, resolution, static_cast<uint8_t>(channel));
}

bool ledcWrite(uint8_t pin, uint32_t duty)
{
    return writeDuty(pin, duty);
}

bool ledcWriteChannel(uint8_t channel, uint32_t duty)
{
    // A channel drives every pin attached to it, so this reaches all of
    // them and not just the first — sharing one channel between two
    // backlights is the case that makes the difference.
    if (channel >= HostArduino::kLedcChannelCount) {
        return false;
    }
    bool written = false;
    for (int pin = 0; pin < HostArduino::kGpioPinCount; ++pin) {
        const HostArduino::AnalogOut *slot = ledcSlot(pin);
        if (slot && slot->channel == channel) {
            written = writeDuty(pin, duty) || written;
        }
    }
    return written;
}

uint32_t ledcWriteTone(uint8_t pin, uint32_t freq)
{
    HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot) {
        return 0;
    }
    if (freq == 0) {
        // Silence, reported as an ordinary duty write — this is what
        // arduino-esp32 does, and it is also `noTone`'s first step.
        (void)writeDuty(pin, 0);
        return 0;
    }
    // arduino-esp32 reconfigures the timer to 10 bits and parks the duty
    // at 0x1FF, a 50% square wave.
    slot->frequency = freq;
    slot->resolution = 10;
    slot->duty = 0x1FF;
    report(HostArduino::kAnalogTone, *slot);
    return freq;
}

uint32_t ledcWriteNote(uint8_t pin, note_t note, uint8_t octave)
{
    // Equal-temperament table for octave 8, halved per octave down —
    // the same integer arithmetic arduino-esp32 uses, so a trace records
    // the frequency the sketch would have produced on silicon.
    static const uint16_t base[12] = {
        4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902,
    };
    if (octave > 8 || note >= NOTE_MAX) {
        return 0;
    }
    const uint32_t freq = static_cast<uint32_t>(base[note]) / static_cast<uint32_t>(1UL << (8 - octave));
    return ledcWriteTone(pin, freq);
}

uint32_t ledcRead(uint8_t pin)
{
    const HostArduino::AnalogOut *slot = ledcSlot(pin);
    return slot ? slot->duty : 0;
}

uint32_t ledcReadFreq(uint8_t pin)
{
    // Faithful to arduino-esp32, including the part that surprises people:
    // a parked channel reads 0 Hz. `HostArduino::analogOut(pin).frequency`
    // is the accessor that reports what was configured.
    const HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot || slot->duty == 0) {
        return 0;
    }
    return slot->frequency;
}

bool ledcDetach(uint8_t pin)
{
    HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot) {
        return false;
    }
    slot->attached = false;
    slot->channel = HostArduino::kNoLedcChannel;
    slot->duty = 0;
    slot->inverted = false;
    report(HostArduino::kAnalogDetach, *slot);
    return true;
}

uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution)
{
    HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot || freq == 0 || resolution == 0 || resolution > HostArduino::kLedcMaxResolution) {
        return 0;
    }
    slot->frequency = freq;
    slot->resolution = resolution;
    report(HostArduino::kAnalogConfig, *slot);
    return freq;
}

bool ledcOutputInvert(uint8_t pin, bool out_invert)
{
    HostArduino::AnalogOut *slot = ledcSlot(pin);
    if (!slot) {
        return false;
    }
    slot->inverted = out_invert;
    report(HostArduino::kAnalogConfig, *slot);
    return true;
}

// Fades land instantly: both endpoints are reported as duty writes and
// `max_fade_time_ms` is ignored. There is nothing to ramp — no waveform is
// emitted — and a fade that took wall-clock time would make a trace depend
// on scheduling. A completion callback therefore fires before the call
// returns, on the calling thread, rather than from the LEDC ISR.
static bool ledcFadeImpl(uint8_t pin, uint32_t start_duty, uint32_t target_duty, void (*userFunc)(void *), void *arg)
{
    if (!writeDuty(pin, start_duty)) {
        return false;
    }
    (void)writeDuty(pin, target_duty);
    if (userFunc) {
        userFunc(arg);
    }
    return true;
}

bool ledcFade(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms)
{
    (void)max_fade_time_ms;
    return ledcFadeImpl(pin, start_duty, target_duty, nullptr, nullptr);
}

bool ledcFadeWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                           void (*userFunc)(void))
{
    (void)max_fade_time_ms;
    // Wrap the no-argument callback so both spellings share one path.
    struct Trampoline {
        static void call(void *fn) { reinterpret_cast<void (*)(void)>(fn)(); }
    };
    if (!userFunc) {
        return ledcFadeImpl(pin, start_duty, target_duty, nullptr, nullptr);
    }
    return ledcFadeImpl(pin, start_duty, target_duty, &Trampoline::call, reinterpret_cast<void *>(userFunc));
}

bool ledcFadeWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                              void (*userFunc)(void *), void *arg)
{
    (void)max_fade_time_ms;
    return ledcFadeImpl(pin, start_duty, target_duty, userFunc, arg);
}

// The gamma curve only shapes the intermediate steps of a fade, and there
// are no intermediate steps here, so the table is accepted and the gamma
// fades behave exactly like the linear ones.
bool ledcSetGammaTable(const float *gamma_table, uint16_t size)
{
    return gamma_table != nullptr && size != 0;
}

void ledcClearGammaTable(void) {}

void ledcSetGammaFactor(float factor)
{
    (void)factor;
}

bool ledcFadeGamma(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms)
{
    return ledcFade(pin, start_duty, target_duty, max_fade_time_ms);
}

bool ledcFadeGammaWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                                void (*userFunc)(void))
{
    return ledcFadeWithInterrupt(pin, start_duty, target_duty, max_fade_time_ms, userFunc);
}

bool ledcFadeGammaWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                                   void (*userFunc)(void *), void *arg)
{
    return ledcFadeWithInterruptArg(pin, start_duty, target_duty, max_fade_time_ms, userFunc, arg);
}

// --- Analog output: analogWrite --------------------------------------

void analogWrite(uint8_t pin, int value)
{
    AnalogTables &t = tables();
    if (!ledcSlot(pin)) {
        // arduino-esp32 attaches on first use with the global defaults,
        // so the trace shows an attach followed by the write.
        if (!ledcAttach(pin, t.write_hz, t.write_bits)) {
            return;
        }
    }
    (void)writeDuty(pin, static_cast<uint32_t>(value));
}

void analogWriteFrequency(uint8_t pin, uint32_t freq)
{
    // Faithful to arduino-esp32, including the surprising part: an
    // attached pin is retuned with the *global* resolution, not the one it
    // was attached with, so this can narrow a pin `ledcAttach` set up at
    // 12 bits down to the analogWrite default of 8.
    AnalogTables &t = tables();
    if (ledcSlot(pin) && ledcChangeFrequency(pin, freq, t.write_bits) == 0) {
        return;
    }
    t.write_hz = freq;
}

void analogWriteFrequency(uint32_t freq)
{
    // Arduino's global spelling: only the default for pins attached later.
    tables().write_hz = freq;
}

void analogWriteResolution(uint8_t pin, uint8_t bits)
{
    // Likewise retuned with the global frequency rather than the pin's.
    AnalogTables &t = tables();
    if (ledcSlot(pin) && ledcChangeFrequency(pin, t.write_hz, bits) == 0) {
        return;
    }
    t.write_bits = bits;
}

void analogWriteResolution(uint8_t bits)
{
    tables().write_bits = bits;
}

// --- Analog output: DAC ----------------------------------------------
//
// A DAC pin is tracked in the same per-pin slot as an LEDC one, with no
// channel and no frequency, so one trace covers both. Which DAC pins a
// real board has is a variant detail the core does not carry, so any pin
// is accepted.

bool dacWrite(uint8_t pin, uint8_t value)
{
    HostArduino::AnalogOut &slot = tables().out[pin];
    slot.channel = HostArduino::kNoLedcChannel;
    slot.frequency = 0;
    slot.resolution = 8;
    slot.duty = value;
    slot.attached = true;
    slot.dac = true;
    report(HostArduino::kAnalogDac, slot);
    return true;
}

bool dacDisable(uint8_t pin)
{
    HostArduino::AnalogOut &slot = tables().out[pin];
    if (!slot.dac) {
        return false;
    }
    slot.attached = false;
    slot.dac = false;
    slot.duty = 0;
    // Frequency and resolution are left as they were, the same way
    // `ledcDetach` leaves them: a detached pin still reports what it was
    // last configured with, which is what a trace wants to show.
    report(HostArduino::kAnalogDetach, slot);
    return true;
}

// --- Analog output: tone ---------------------------------------------

void tone(uint8_t pin, unsigned int frequency, unsigned long duration)
{
    AnalogTables &t = tables();
    if (t.tone_pin >= 0 && t.tone_pin != static_cast<int>(pin)) {
        HOST_DIAG_ONCE("tone() is already running on another pin; call noTone() first");
        return;
    }
    if (t.tone_pin < 0) {
        if (!ledcAttach(pin, frequency, 10)) {
            HOST_DIAG_ONCE("tone() could not attach an LEDC channel");
            return;
        }
        t.tone_pin = static_cast<int>(pin);
    }
    (void)ledcWriteTone(pin, frequency);

    // arduino-esp32 times the note on its tone task and returns at once.
    // The host collapses the note to zero length instead of blocking the
    // caller: the trace still records the tone and the silence in order,
    // and a sketch shared with real silicon keeps the same call sequence.
    if (duration) {
        (void)ledcWriteTone(pin, 0);
    }
}

void noTone(uint8_t pin)
{
    AnalogTables &t = tables();
    if (t.tone_pin != static_cast<int>(pin)) {
        HOST_DIAG_ONCE("noTone() called for a pin that tone() is not running on");
        return;
    }
    (void)ledcWriteTone(pin, 0);
    (void)ledcDetach(pin);
    t.tone_pin = -1;
}
