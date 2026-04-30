#ifndef HOST_ARDUINO_H
#define HOST_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <string>

typedef uint8_t byte;
typedef bool boolean;
typedef std::string String;

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

#define bit(b) (1UL << (b))

extern void setup();
extern void loop();

#include "HostRuntime.h"

#endif
