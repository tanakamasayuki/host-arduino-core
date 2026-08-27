#include "Arduino.h"

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
