#ifndef HOST_ARDUINO_BUS_H
#define HOST_ARDUINO_BUS_H

#include <stddef.h>
#include <stdint.h>

// Bus observation port — GPIO half.
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
// The SPI half of the port lives on `SPIClass` in the bundled SPI library
// (`SPI.setTransferHook`), the I2C half on `TwoWire` in the bundled Wire
// library (`Wire.setWriteHook` / `setReadHook`).
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
// `digitalWrite` / `digitalRead`, and the pin state is a plain array with
// no locking. With `mode=lgfx` (and on the `display` board) `setup()` /
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

} // namespace HostArduino

#endif
