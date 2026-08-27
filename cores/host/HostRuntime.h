#ifndef HOST_ARDUINO_RUNTIME_H
#define HOST_ARDUINO_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include <chrono>
#include <string>
#include <thread>

#include "HostBus.h"
#include "Stream.h"

namespace HostArduino {

bool runtimeStart(int argc, char **argv);
void runtimeStop();
bool runtimeShouldStop();
void runtimePoll();
void runtimeLogInfo(const char *event, const char *message = nullptr);
size_t serialWrite(const char *data, size_t len);
int serialAvailable();
int serialRead();
int serialPeek();

} // namespace HostArduino

class SerialClass : public Stream {
public:
    void begin(unsigned long) {}
    void begin(unsigned long, uint32_t) {}
    void end() {}

    size_t write(uint8_t value)
    {
        const char ch = static_cast<char>(value);
        return HostArduino::serialWrite(&ch, 1);
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        return HostArduino::serialWrite(reinterpret_cast<const char *>(buffer), size);
    }

    size_t write(const char *buffer, size_t size)
    {
        return HostArduino::serialWrite(buffer, size);
    }

    int available()
    {
        return HostArduino::serialAvailable();
    }

    int read()
    {
        return HostArduino::serialRead();
    }

    int peek()
    {
        return HostArduino::serialPeek();
    }

    int availableForWrite()
    {
        return 1024;
    }

    void flush() {}

    operator bool() const
    {
        return true;
    }
};

extern SerialClass Serial;

inline uint32_t millis()
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

inline uint32_t micros()
{
    static const auto start = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - start).count());
}

inline void delay(unsigned long ms)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!HostArduino::runtimeShouldStop() && std::chrono::steady_clock::now() < end) {
        HostArduino::runtimePoll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

inline void delayMicroseconds(unsigned int us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void yield()
{
    HostArduino::runtimePoll();
    std::this_thread::yield();
}

// GPIO — see cores/host/HostBus.h for what the pin state and the hooks
// are for. `digitalWrite` / `digitalRead` stay inline here because
// bit-banged transports call them millions of times per frame; the state
// and the hook pointers live in HostBus.cpp so every translation unit
// shares one set.
inline void pinMode(int pin, int mode)
{
    HostArduino::bus_detail::applyPinMode(pin, mode);
}

inline void digitalWrite(int pin, int value)
{
    if (static_cast<unsigned>(pin) >= static_cast<unsigned>(HostArduino::kGpioPinCount)) {
        return;
    }
    const uint8_t level = value ? 1 : 0;
    HostArduino::bus_detail::pin_value[pin] = level;
    if (HostArduino::bus_detail::pin_write_hook) {
        HostArduino::bus_detail::pin_write_hook(static_cast<uint8_t>(pin), level,
                                               HostArduino::bus_detail::pin_write_hook_user);
    }
}

inline int digitalRead(int pin)
{
    if (static_cast<unsigned>(pin) >= static_cast<unsigned>(HostArduino::kGpioPinCount)) {
        return 0;
    }
    const uint8_t held = HostArduino::bus_detail::pin_value[pin];
    if (HostArduino::bus_detail::pin_read_hook) {
        return HostArduino::bus_detail::pin_read_hook(static_cast<uint8_t>(pin), held,
                                                     HostArduino::bus_detail::pin_read_hook_user);
    }
    return held;
}

inline int analogRead(int) { return 0; }
inline void analogWrite(int, int) {}
inline void analogReadResolution(uint8_t) {}
inline void analogWriteResolution(uint8_t) {}
inline void analogSetAttenuation(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline int touchRead(int) { return 0; }
inline void noTone(int) {}
inline void tone(int, unsigned int, unsigned long = 0) {}
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int, void (*)(void), int) {}
inline void detachInterrupt(int) {}
inline unsigned long pulseIn(int, int, unsigned long = 1000000UL) { return 0; }
inline unsigned long pulseInLong(int, int, unsigned long = 1000000UL) { return 0; }

#endif
