#ifndef HOST_ARDUINO_SERVO_H
#define HOST_ARDUINO_SERVO_H

// Servo, with the Arduino Servo library's API (which ESP32Servo copies).
//
// There is no motor here. What a servo driver actually does on an ESP32 is
// produce a 50 Hz PWM whose high time is the pulse width, so that is what
// this does, through the LEDC functions of this core:
//
//   attach(pin)           ledcAttach(pin, 50, 16)      -> kAnalogAttach
//   write / writeMicros   ledcWrite(pin, duty)         -> kAnalogWrite
//   detach()              ledcDetach(pin)              -> kAnalogDetach
//
// so a test sees servo motion through HostArduino::setAnalogWriteHook in
// the same ordered trace as the rest of the bus. duty = pulse_us * 65536 /
// 20000; `pulseFromDuty` turns it back.
//
// `write(v)` follows the Arduino library: v below MIN_PULSE_WIDTH is an
// angle (clamped to 0..180), anything else is a pulse width in us.

#include <Arduino.h>

#ifndef MIN_PULSE_WIDTH
#define MIN_PULSE_WIDTH 544
#endif
#ifndef MAX_PULSE_WIDTH
#define MAX_PULSE_WIDTH 2400
#endif
#ifndef DEFAULT_PULSE_WIDTH
#define DEFAULT_PULSE_WIDTH 1500
#endif
#ifndef REFRESH_INTERVAL
#define REFRESH_INTERVAL 20000
#endif
#ifndef MAX_SERVOS
#define MAX_SERVOS 16
#endif
#ifndef INVALID_SERVO
#define INVALID_SERVO 255
#endif

class Servo {
public:
    static constexpr uint32_t kFrequency = 50;
    static constexpr uint8_t kResolution = 16;

    Servo() {}

    // Returns the channel-ish index the Arduino library returns: 0 on
    // success here (one servo per object), INVALID_SERVO on failure.
    uint8_t attach(int pin) { return attach(pin, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH); }
    uint8_t attach(int pin, int min, int max)
    {
        if (pin < 0) {
            return INVALID_SERVO;
        }
        if (_pin >= 0) {
            detach();
        }
        if (!ledcAttach(static_cast<uint8_t>(pin), kFrequency, kResolution)) {
            return INVALID_SERVO;
        }
        _pin = pin;
        _min = min < MIN_PULSE_WIDTH ? MIN_PULSE_WIDTH : min;
        _max = max > MAX_PULSE_WIDTH ? MAX_PULSE_WIDTH : max;
        if (_max < _min) {
            _max = _min;
        }
        return 0;
    }

    void detach()
    {
        if (_pin < 0) {
            return;
        }
        ledcDetach(static_cast<uint8_t>(_pin));
        _pin = -1;
    }

    void write(int value)
    {
        if (value < MIN_PULSE_WIDTH) {
            if (value < 0) value = 0;
            if (value > 180) value = 180;
            value = _min + static_cast<int>(static_cast<long>(value) * (_max - _min) / 180);
        }
        writeMicroseconds(value);
    }

    void writeMicroseconds(int us)
    {
        if (us < _min) us = _min;
        if (us > _max) us = _max;
        _us = us;
        if (_pin >= 0) {
            ledcWrite(static_cast<uint8_t>(_pin), dutyFromPulse(static_cast<uint32_t>(us)));
        }
    }

    // Angle 0..180 recovered from the pulse width, as the Arduino library
    // reports it (rounded to the nearest degree).
    int read() const
    {
        if (_max == _min) return 0;
        return static_cast<int>((static_cast<long>(_us - _min) * 180 + (_max - _min) / 2) / (_max - _min));
    }
    int readMicroseconds() const { return _us; }
    bool attached() const { return _pin >= 0; }
    int pin() const { return _pin; }

    static uint32_t dutyFromPulse(uint32_t us)
    {
        return static_cast<uint32_t>((static_cast<uint64_t>(us) << kResolution) / REFRESH_INTERVAL);
    }
    static uint32_t pulseFromDuty(uint32_t duty)
    {
        return static_cast<uint32_t>((static_cast<uint64_t>(duty) * REFRESH_INTERVAL + (1u << (kResolution - 1))) >> kResolution);
    }

private:
    int _pin = -1;
    int _min = MIN_PULSE_WIDTH;
    int _max = MAX_PULSE_WIDTH;
    int _us = DEFAULT_PULSE_WIDTH;
};

#endif
