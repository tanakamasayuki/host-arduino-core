#ifndef HOST_ARDUINO_BUS_H
#define HOST_ARDUINO_BUS_H

#include <stddef.h>
#include <stdint.h>

// Bus observation port — GPIO and analog / PWM halves.
//
// The core deliberately does not model peripherals. The code that knows a
// device's protocol is the library that drives that device (an ST7789
// command sequence belongs to the display library, the SD command set to
// the SD library), so the core would never stop growing if it started
// collecting device models. What it provides instead is a place to watch
// the bus from:
//
//   - every `digitalWrite` is announced to a hook,
//   - every pin remembers the last value written to it,
//   - `digitalRead` hands that value back.
//
// A library keeps its own device model on the library side and drives it
// from what it sees on the pins. This is what makes bit-banged transports
// testable on the host: soft SPI, soft I2C, WS2812, IR — none of them go
// through a bus class, they only ever touch `digitalWrite`.
//
// The analog / PWM half is further down this header: `analogWrite`, the
// `ledc*` family, `dacWrite`, and `tone` record per-pin state and announce
// it to one hook, so a backlight being configured and lit shows up in the
// same ordered trace as the pin writes around it.
//
// The SPI half of the port lives on `SPIClass` in the bundled SPI library
// (`SPI.setTransferHook`), the I2C half on `TwoWire` in the bundled Wire
// library (`Wire.setWriteHook` / `setReadHook`). Both also announce
// `begin()` / `end()` and their configuration setters to a lifecycle hook,
// which is what puts bus initialization into that same trace.
//
// Cost. A 240x240 16bpp frame pushed out over bit-banged SPI is roughly
// 2.8 million `digitalWrite` calls (115,200 bytes x 8 bits x MOSI + two
// SCK edges), so the write path stays inline in this header and the hook
// is a plain function pointer — not a `std::function`, which would add an
// indirection and a heap allocation to something this hot.
//
// Timestamps are deliberately absent from the hook signature: `micros()`
// is already available and monotonic, so a hook that cares can call it
// itself and one that does not pays nothing.
//
// Threading. Hooks are called synchronously on whichever thread called
// `digitalWrite` / `digitalRead` / `analogWrite`, and the state is plain
// arrays with no locking. With `mode=lgfx` (and on the `display` board) `setup()` /
// `loop()` run on a worker thread, and FreeRTOS tasks are `std::thread`,
// so register hooks before starting tasks and keep one bus per thread.

namespace HostArduino {

// Pin numbers 0..255 are tracked. Fixed at compile time on purpose: the
// bound check is inlined into sketch translation units while the array
// itself lives in the core archive, so a per-translation-unit override
// would let a sketch write past the end of it. Writes to pins outside
// the range are dropped (no hook call) and reads return 0.
constexpr int kGpioPinCount = 256;

// Called after the pin's stored value has been updated. `value` is
// already normalized to 0 / 1.
using PinWriteHook = void (*)(uint8_t pin, uint8_t value, void *user);

// Called after the pin's mode has been recorded. `mode` is the raw
// Arduino constant (`INPUT`, `OUTPUT`, `INPUT_PULLUP`, ...).
using PinModeHook = void (*)(uint8_t pin, uint8_t mode, void *user);

// Called by `digitalRead` instead of returning the stored value. `held`
// is what `digitalRead` would have returned; the return value is what the
// sketch sees. Use this when a device model has to compute an input level
// at read time (a busy flag, a bit-banged MISO line); for a level the
// model already knows, `setPinValue` is cheaper.
using PinReadHook = int (*)(uint8_t pin, uint8_t held, void *user);

namespace bus_detail {

// Defined in HostBus.cpp. Exposed because `digitalWrite` / `digitalRead`
// are inline in HostRuntime.h; treat as core-internal.
extern uint8_t pin_value[kGpioPinCount];
extern uint8_t pin_mode[kGpioPinCount];
extern PinWriteHook pin_write_hook;
extern void *pin_write_hook_user;
extern PinReadHook pin_read_hook;
extern void *pin_read_hook_user;
extern PinModeHook pin_mode_hook;
extern void *pin_mode_hook_user;

void applyPinMode(int pin, int mode);

} // namespace bus_detail

// Hook registration. Passing `nullptr` unregisters. Registering replaces
// the previous hook — there is one slot per kind, on purpose: a chain
// would need allocation, and a device model that needs to share the bus
// can dispatch on the pin number itself.
void setPinWriteHook(PinWriteHook hook, void *user = nullptr);
void setPinModeHook(PinModeHook hook, void *user = nullptr);
void setPinReadHook(PinReadHook hook, void *user = nullptr);
void clearPinHooks();

// Last value written to `pin`, whether by the sketch through
// `digitalWrite` or injected through `setPinValue`. Never consults the
// read hook, so a hook can call this to see the held level.
uint8_t pinValue(int pin);

// Inject an input level. This is the response direction for GPIO: a
// device model that has decoded a command writes its answer here and the
// sketch's next `digitalRead` picks it up. Because everything is
// synchronous, ordering between the sketch's writes and the model's
// answers is naturally preserved.
void setPinValue(int pin, uint8_t value);

// Mode last passed to `pinMode`, or `INPUT` (0) if it was never called.
uint8_t pinModeOf(int pin);

// Reset every pin to value 0 / mode `INPUT` without touching the hooks.
void resetPinState();

// --- Analog / PWM half -----------------------------------------------
//
// `analogWrite`, the `ledc*` family, `dacWrite`, and `tone` all end up
// driving one pin's analog output. On silicon that is a peripheral; here
// it is a recorded state plus one hook, for the same reason the GPIO half
// exists: what a test needs to see is *that the sketch configured the
// backlight at 5 kHz / 8 bits and then wrote duty 128*, in that order,
// next to everything else it put on the bus.
//
// One hook covers the whole family instead of one slot per call, because
// the thing being asserted is an ordered sequence. A golden trace wants a
// single stream; four streams would have to be re-interleaved by the test
// before it could compare anything.
//
// The events are coarser than the API — `analogWrite` on an unattached
// pin reports an attach and then a write, exactly as arduino-esp32
// performs them — so the trace records what happened to the pin, not
// which spelling the caller reached for. The values in `AnalogOut` are
// what distinguish the cases that matter.
//
// Not modelled: waveforms, timing, and LEDC's timer allocation. Nothing
// is emitted on the pin, `digitalRead` does not see a PWM signal, and a
// duty of 128/255 does not make anything 50% bright. Frequencies and
// resolutions are recorded and range-checked the way silicon would reject
// them, never honored. Silicon can also refuse an attach because no timer
// with a matching frequency and resolution is free; the host never runs
// out of timers, so a sketch that would have hit that limit still
// succeeds here — only the channels are finite.

// LEDC channels tracked, using the classic ESP32 count (two groups of
// eight). Same choice the bundled SPI library makes for FSPI / HSPI /
// VSPI: one variant's constants rather than a per-chip map.
//
// Several pins may share one channel, as on silicon: attaching a second
// pin to a used channel adopts that channel's frequency and resolution,
// and `ledcWriteChannel` then writes the duty to every pin on it.
constexpr int kLedcChannelCount = 16;

// Widest duty resolution accepted, matching `LEDC_TIMER_20_BIT` on the
// classic ESP32. Attaching with more bits fails, as it does on silicon.
constexpr uint8_t kLedcMaxResolution = 20;

// `AnalogOut::channel` when no LEDC channel is involved — a pin that has
// never been attached, or a `dacWrite`.
constexpr uint8_t kNoLedcChannel = 0xFF;

// What happened to the pin. The call that produced each event:
//
//   kAnalogAttach  ledcAttach, ledcAttachChannel, and the implicit attach
//                  inside analogWrite / tone on an unattached pin
//   kAnalogWrite   ledcWrite, ledcWriteChannel, analogWrite, and both
//                  endpoints of ledcFade*
//   kAnalogConfig  ledcChangeFrequency, ledcOutputInvert,
//                  analogWriteFrequency, analogWriteResolution
//   kAnalogTone    ledcWriteTone, ledcWriteNote, tone
//   kAnalogDetach  ledcDetach, noTone, dacDisable
//   kAnalogDac     dacWrite
//
// A call that silicon would reject (duty on an unattached pin, a
// resolution wider than `kLedcMaxResolution`, a zero frequency) changes
// no state and reports no event, so a trace does not show work that a
// real board would have refused.
enum AnalogWriteEvent : uint8_t {
    kAnalogAttach = 0,
    kAnalogWrite,
    kAnalogConfig,
    kAnalogTone,
    kAnalogDetach,
    kAnalogDac,
};

// Everything the core knows about one pin's analog output, handed to the
// hook after the call has been applied.
struct AnalogOut {
    uint8_t pin = 0;
    // Channel the pin is attached to, or `kNoLedcChannel`. `ledcAttach`
    // assigns the lowest free channel, the same way arduino-esp32 does,
    // so a trace shows the channel the sketch would have got on silicon.
    uint8_t channel = kNoLedcChannel;
    // Last duty written. `ledcWrite` clamps a full-scale duty up to
    // 2^resolution, matching the LEDC "full on" fixup.
    uint32_t duty = 0;
    uint32_t frequency = 0; // Hz; 0 for a DAC pin
    uint8_t resolution = 0; // duty bits; 8 for a DAC pin
    bool attached = false;
    bool inverted = false; // ledcOutputInvert
    bool dac = false;      // last written through dacWrite
};

using AnalogWriteHook = void (*)(AnalogWriteEvent event, const AnalogOut &out, void *user);

// Called by `analogRead` instead of returning the injected value. `held`
// is what `analogRead` would have returned; the return value is what the
// sketch sees. The millivolt reading has no hook of its own —
// `analogReadMilliVolts` always reports what `setAnalogMilliVolts` last
// injected, because the core has no attenuation or Vref model to derive
// one from.
using AnalogReadHook = uint16_t (*)(uint8_t pin, uint16_t held, void *user);

// The same for the millivolt reading, as its own hook rather than a flag
// on the one above: the two are independent quantities here, and folding
// them together would have changed a signature that is already in use.
// `held` is what `setAnalogMilliVolts` last injected; the return value is
// what the sketch sees. The core still derives nothing — there is no
// attenuation or Vref model behind either reading, which is exactly why
// both are injected separately.
using AnalogMilliVoltsHook = uint32_t (*)(uint8_t pin, uint32_t held, void *user);

// Called after `analogReadResolution` / `analogSetWidth` has recorded the
// new width, with that width. There is no event enum because the width is
// the only read-side configuration this core has; the two spellings are
// the same knob, so they are not told apart.
//
// This is a second hook rather than an event on `setAnalogWriteHook`, and
// nothing is lost by that: both are called synchronously on the sketch's
// thread, so a driver appending to one buffer from both keeps the order.
// Adding a value to `AnalogWriteEvent` would also have broken every
// exhaustive `switch` over it that has no `default`.
using AnalogReadConfigHook = void (*)(uint8_t bits, void *user);

// Hook registration. As with the GPIO half: `nullptr` unregisters, and
// registering replaces the previous hook.
void setAnalogWriteHook(AnalogWriteHook hook, void *user = nullptr);
void setAnalogReadHook(AnalogReadHook hook, void *user = nullptr);
void setAnalogMilliVoltsHook(AnalogMilliVoltsHook hook, void *user = nullptr);
void setAnalogReadConfigHook(AnalogReadConfigHook hook, void *user = nullptr);
// Releases all four slots.
void clearAnalogHooks();

// Analog output state of `pin`. Out-of-range pins report a zeroed,
// unattached state. This is the accessor to assert against when a test
// only wants the end state and not the sequence — `ledcRead` /
// `ledcReadFreq` answer the way silicon does (frequency reads 0 while the
// duty is 0), which is faithful but hides what was configured.
const AnalogOut &analogOut(int pin);

// Lowest-numbered pin attached to an LEDC channel, or -1 when the channel
// is free. When several pins share a channel this reports the first of
// them; `ledcWriteChannel` writes to all of them.
int ledcChannelPin(int channel);

// Inject an ADC reading. The response direction for the analog half, the
// counterpart of `setPinValue`.
void setAnalogValue(int pin, uint16_t value);
uint16_t analogValue(int pin);

// Inject the millivolt reading `analogReadMilliVolts` reports. Kept
// separate from the raw value on purpose: deriving one from the other
// needs an attenuation and Vref model the core does not have, and a
// sketch that reads both should be able to be given both.
void setAnalogMilliVolts(int pin, uint32_t millivolts);
uint32_t analogMilliVolts(int pin);

// Global defaults, as the last resolution / frequency call left them.
// `analogWrite` attaches an unattached pin with these.
uint8_t analogReadBits();
uint8_t analogWriteBits();
uint32_t analogWriteHz();

// Reset every pin's analog output (which frees every channel), the
// injected readings, and the global defaults, without touching the hooks.
void resetAnalogState();

} // namespace HostArduino

#endif
