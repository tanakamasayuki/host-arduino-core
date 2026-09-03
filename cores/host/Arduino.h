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

#ifndef LSBFIRST
#define LSBFIRST 0
#endif
#ifndef MSBFIRST
#define MSBFIRST 1
#endif

#ifndef CHANGE
#define CHANGE 1
#endif
#ifndef FALLING
#define FALLING 2
#endif
#ifndef RISING
#define RISING 3
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

#endif
