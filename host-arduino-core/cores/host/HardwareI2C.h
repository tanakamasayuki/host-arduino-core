#ifndef HOST_ARDUINO_HARDWARE_I2C_H
#define HOST_ARDUINO_HARDWARE_I2C_H

// The I2C base class, in arduino-esp32's shape (cores/esp32/HardwareI2C.h
// in 3.x): `bool` returns and `std::function` callbacks. ArduinoCore-API
// declares the same members with `void` returns and plain function
// pointers; code that *calls* through a `HardwareI2C &` cannot tell the
// two apart, and that is what a driver taking "any I2C bus" does.
//
// A library that *derives* from it (a software I2C) must match these
// signatures, exactly as it would have to on an ESP32.
//
// Reachable as `arduino::HardwareI2C` too, the ArduinoCore-API spelling.

#include "Stream.h"
#include <stddef.h>
#include <stdint.h>
#include <functional>

class HardwareI2C : public Stream {
public:
    virtual bool begin() = 0;
    virtual bool begin(uint8_t address) = 0;
    virtual bool end() = 0;

    virtual bool setClock(uint32_t freq) = 0;

    virtual void beginTransmission(uint8_t address) = 0;
    virtual uint8_t endTransmission(bool stopBit) = 0;
    virtual uint8_t endTransmission(void) = 0;

    virtual size_t requestFrom(uint8_t address, size_t len, bool stopBit) = 0;
    virtual size_t requestFrom(uint8_t address, size_t len) = 0;

    virtual void onReceive(const std::function<void(int)> &) = 0;
    virtual void onRequest(const std::function<void()> &) = 0;
};

namespace arduino {
using ::HardwareI2C;
} // namespace arduino

#endif
