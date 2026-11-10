#ifndef HOST_ARDUINO_H
#define HOST_ARDUINO_H

// On Windows, prevent <windows.h> (pulled in transitively by openssl,
// winsock, SDL, etc.) from declaring identifiers that collide with the
// Arduino API. Must precede every other include in this file so the
// guards are in effect by the time any header chain reaches windows.h.
//
//   NOUSER        — skip <winuser.h>, which declares `struct INPUT`
//                   (we use INPUT as a pinMode constant).
//   WIN32_LEAN_AND_MEAN — also drops RPC, which declares
//                   `typedef unsigned char boolean` (we use bool).
//   NOGDI         — drops GDI; not strictly required, just lighter.
//   NOMINMAX      — drops min/max macros; we provide templates.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>
// MinGW's sys/stat.h declares mkdir with one argument only (no mode_t).
// Provide the POSIX two-argument overload so sketches can use mkdir(path, mode).
inline int mkdir(const char *path, int /*mode*/) { return _mkdir(path); }
#endif

typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

#include "WString.h"
#include "Print.h"
#include "Stream.h"
#include "pgmspace.h"
#include "IPAddress.h"

#ifndef HIGH
#define HIGH 0x1
#endif
#ifndef LOW
#define LOW 0x0
#endif
#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif
#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN 0x3
#endif
#ifndef OUTPUT_OPEN_DRAIN
#define OUTPUT_OPEN_DRAIN 0x4
#endif

// ArduinoCore-API spelling. `OUTPUT_OPEN_DRAIN` above is the arduino-esp32
// one; both name the same mode here.
#ifndef OUTPUT_OPENDRAIN
#define OUTPUT_OPENDRAIN OUTPUT_OPEN_DRAIN
#endif

// Bit order is an enum, as in ArduinoCore-API, rather than the two plain
// macros arduino-esp32 uses. A library written against the API declares
// parameters of type `BitOrder` and passes `MSBFIRST` into them (Adafruit
// BusIO does exactly this when the platform is neither AVR nor ESP32), and
// an int macro cannot convert to an enum. The values are unchanged, an
// unscoped enumerator still converts to `uint8_t` / `int` wherever the
// ESP32 spelling expects one, and the macros are kept defined (to
// themselves) so `#ifdef MSBFIRST` still holds. If something defined them
// before this header, that definition wins and no enum is declared.
#if !defined(LSBFIRST) && !defined(MSBFIRST)
typedef enum {
    LSBFIRST = 0,
    MSBFIRST = 1,
} BitOrder;
#define LSBFIRST LSBFIRST
#define MSBFIRST MSBFIRST
#define HOST_ARDUINO_HAS_BITORDER_ENUM 1
#else
typedef uint8_t BitOrder;
#endif

// ArduinoCore-API names for pin values and modes. They stay integers here:
// `digitalRead` returns `int` and HIGH / LOW / INPUT / ... are macros with
// this core's long-standing numbering, so an enum would either refuse
// `PinStatus s = digitalRead(pin)` or renumber CHANGE / FALLING / RISING.
typedef int PinStatus;
typedef int PinMode;
typedef uint8_t pin_size_t;

#ifndef CHANGE
#define CHANGE 1
#endif
#ifndef FALLING
#define FALLING 2
#endif
#ifndef RISING
#define RISING 3
#endif

// Level triggers, at arduino-esp32's values — they do not collide with the
// three above, which keep the AVR numbering this core has always used.
//
// Careful: those three do NOT agree with arduino-esp32, where RISING is 1
// and CHANGE is 3. The numbers are host-local, and every mode this core
// records is also reported as a normalized `HostArduino::InterruptTrigger`
// so nothing has to compare them — see cores/host/HostInterrupt.h.
#ifndef ONLOW
#define ONLOW 0x04
#endif
#ifndef ONHIGH
#define ONHIGH 0x05
#endif
#ifndef ONLOW_WE
#define ONLOW_WE 0x0C
#endif
#ifndef ONHIGH_WE
#define ONHIGH_WE 0x0D
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#ifndef HALF_PI
#define HALF_PI 1.5707963267948966192313216916398
#endif
#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876798154814105
#endif
#ifndef EULER
#define EULER 2.718281828459045235360287471352
#endif

#define bit(b) (1UL << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define sq(x) ((x) * (x))

#ifndef _BV
#define _BV(b) (1UL << (b))
#endif

// CPU clock. There is no CPU clock to speak of on the host, so this is the
// classic ESP32's default (240 MHz) — the value an ESP32 sketch would see —
// and the conversions below are arduino-esp32's, in terms of it. Drivers
// that busy-wait in cycles (DHT and friends) use these to size loops; they
// compile and stay self-consistent, they do not time anything.
#ifndef F_CPU
#define F_CPU 240000000U
#endif
inline uint32_t getCpuFrequencyMhz() { return F_CPU / 1000000U; }
#ifndef clockCyclesPerMicrosecond
#define clockCyclesPerMicrosecond() ((long int)getCpuFrequencyMhz())
#endif
#ifndef clockCyclesToMicroseconds
#define clockCyclesToMicroseconds(a) ((a) / clockCyclesPerMicrosecond())
#endif
#ifndef microsecondsToClockCycles
#define microsecondsToClockCycles(a) ((a) * clockCyclesPerMicrosecond())
#endif

#ifndef NOT_AN_INTERRUPT
#define NOT_AN_INTERRUPT -1
#endif

// Placement attributes from esp_attr.h. Memory placement means nothing on
// the host; empty so `void IRAM_ATTR isr()` compiles unchanged.
#include "esp_attr.h"

// arduino-esp32 compatibility. On the ESP32 core these macros override weak
// functions the runtime consults during startup. The host build has no loop
// task / startup wait / chip report, so the defined functions are simply never
// called (no-op). Defined verbatim so sketches using them still build here.
#define SET_LOOP_TASK_STACK_SIZE(sz)     \
  size_t getArduinoLoopTaskStackSize() { \
    return sz;                           \
  }

#define SET_TIME_BEFORE_STARTING_SKETCH_MS(time_ms) \
  uint64_t getArduinoSetupWaitTime_ms() {           \
    return (time_ms);                               \
  }

#define ENABLE_CHIP_DEBUG_REPORT          \
  bool shouldPrintChipDebugReport(void) { \
    return true;                          \
  }

extern void setup();
extern void loop();

#include "HostRuntime.h"
#include "HostInterrupt.h"
#include "HostLifecycle.h"
#include "HostUart.h"
#include "esp32-hal-log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef SerialClass HardwareSerial;

template <typename T>
inline const T &min(const T &a, const T &b)
{
    return b < a ? b : a;
}

template <typename T>
inline const T &max(const T &a, const T &b)
{
    return a < b ? b : a;
}

template <typename T>
inline T constrain(T value, T low, T high)
{
    return value < low ? low : (value > high ? high : value);
}

inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline long random(long max)
{
    return max <= 0 ? 0 : std::rand() % max;
}

inline long random(long min, long max)
{
    return min >= max ? min : min + random(max - min);
}

inline void randomSeed(unsigned long seed)
{
    std::srand(static_cast<unsigned int>(seed));
}

inline uint16_t makeWord(uint16_t w) { return w; }
inline uint16_t makeWord(uint8_t h, uint8_t l) { return (static_cast<uint16_t>(h) << 8) | l; }

// ArduinoCore-API puts its types in `namespace arduino`, and libraries
// written against it spell them `arduino::Stream` and so on. This core
// keeps them at global scope (as arduino-esp32 does) and makes the same
// types reachable under the API's names. They are the same types, not
// copies, so an `arduino::Stream &` binds to `Serial`. HardwareI2C and
// HardwareSPI join the namespace from their own headers.
namespace arduino {
using ::BitOrder;
using ::HardwareSerial;
using ::IPAddress;
using ::PinMode;
using ::PinStatus;
using ::Print;
using ::Printable;
using ::Stream;
using ::String;
using ::pin_size_t;
} // namespace arduino

#endif
