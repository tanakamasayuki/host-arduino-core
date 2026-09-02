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

// Analog — see the analog / PWM half of cores/host/HostBus.h. These are
// declared here and defined in HostBus.cpp rather than inline: unlike
// `digitalWrite` none of them sits on a bit-banging path, so keeping them
// out of every translation unit is worth more than saving an indirect
// call. Signatures follow arduino-esp32 so the same source builds for
// both targets.

// ADC. Reads come from `HostArduino::setAnalogValue` /
// `setAnalogMilliVolts`, or from an analog read hook; an un-injected pin
// reads 0. The resolution is recorded, never applied — the injected value
// is returned as it was given, because scaling it would mean inventing a
// reference the core does not have.
uint16_t analogRead(uint8_t pin);
uint32_t analogReadMilliVolts(uint8_t pin);
void analogReadResolution(uint8_t bits);
void analogSetWidth(uint8_t bits);
inline void analogSetAttenuation(int) {}
inline void analogSetPinAttenuation(int, int) {}
inline int touchRead(int) { return 0; }

// PWM via LEDC. Frequencies, resolutions, and channel assignments are
// recorded and range-checked the way silicon would reject them; nothing
// is emitted on the pin. Refusals match arduino-esp32: a duty write to an
// unattached pin fails, a second attach of the same pin fails, and a
// resolution wider than `HostArduino::kLedcMaxResolution` fails.
typedef enum {
    NOTE_C,
    NOTE_Cs,
    NOTE_D,
    NOTE_Eb,
    NOTE_E,
    NOTE_F,
    NOTE_Fs,
    NOTE_G,
    NOTE_Gs,
    NOTE_A,
    NOTE_Bb,
    NOTE_B,
    NOTE_MAX
} note_t;

bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);
bool ledcAttachChannel(uint8_t pin, uint32_t freq, uint8_t resolution, uint8_t channel);
bool ledcWrite(uint8_t pin, uint32_t duty);
bool ledcWriteChannel(uint8_t channel, uint32_t duty);
uint32_t ledcWriteTone(uint8_t pin, uint32_t freq);
uint32_t ledcWriteNote(uint8_t pin, note_t note, uint8_t octave);
uint32_t ledcRead(uint8_t pin);
uint32_t ledcReadFreq(uint8_t pin);
bool ledcDetach(uint8_t pin);
uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution);
bool ledcOutputInvert(uint8_t pin, bool out_invert);

// Fades land on the target duty immediately and `max_fade_time_ms` is
// ignored; both endpoints are reported to the hook. A completion callback
// runs before the call returns, on the calling thread. The gamma table is
// accepted and has no effect, since it only shapes intermediate steps.
bool ledcFade(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms);
bool ledcFadeWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                           void (*userFunc)(void));
bool ledcFadeWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                              void (*userFunc)(void *), void *arg);
bool ledcSetGammaTable(const float *gamma_table, uint16_t size);
void ledcClearGammaTable(void);
void ledcSetGammaFactor(float factor);
bool ledcFadeGamma(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms);
bool ledcFadeGammaWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                                void (*userFunc)(void));
bool ledcFadeGammaWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms,
                                   void (*userFunc)(void *), void *arg);

// `analogWrite` attaches an unattached pin on first use with the global
// defaults (1000 Hz, 8 bits), exactly as arduino-esp32 does, so the hook
// sees an attach followed by a write. The pin overloads of
// `analogWriteFrequency` / `analogWriteResolution` retune an attached pin
// and otherwise move the global default; the single-argument spellings are
// the Arduino-generic ones and only ever move the default.
void analogWrite(uint8_t pin, int value);
void analogWriteFrequency(uint8_t pin, uint32_t freq);
void analogWriteFrequency(uint32_t freq);
void analogWriteResolution(uint8_t pin, uint8_t bits);
void analogWriteResolution(uint8_t bits);

// DAC. Any pin is accepted — which pins a real board wires to a DAC is a
// variant detail the core does not carry.
bool dacWrite(uint8_t pin, uint8_t value);
bool dacDisable(uint8_t pin);

// One tone at a time, like arduino-esp32: `tone` on a second pin is
// refused until `noTone`. A non-zero `duration` does not block — the host
// reports the tone and the silence back to back so a shared sketch keeps
// the same call sequence on both targets without a wall-clock wait.
void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t pin);

inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int, void (*)(void), int) {}
inline void detachInterrupt(int) {}
inline unsigned long pulseIn(int, int, unsigned long = 1000000UL) { return 0; }
inline unsigned long pulseInLong(int, int, unsigned long = 1000000UL) { return 0; }

#endif
