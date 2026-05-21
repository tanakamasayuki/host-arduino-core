#ifndef HOST_ARDUINO_HOST_DIAG_H
#define HOST_ARDUINO_HOST_DIAG_H

// Diagnostic hint helpers for host-arduino-core classes.
//
// `arduino-cli upload` detaches the host executable, so stdout / stderr
// from the running sketch are not captured by pytest. Hints therefore go
// through `Serial` (TCP), which `dut.expect()` can observe. The prefix
// "[HostCore] " makes them easy to grep and to skip when needed.
//
// `HOST_DIAG_ONCE` fires at most once per call site (per translation
// unit) so tight `loop()` callers don't flood the wire. Adopt this helper
// in any class that silently returns failure but where the cause is
// surprising — typical Arduino sketches expect Serial.print on errors.

#include "HostRuntime.h" // SerialClass / Serial

#define HOST_DIAG_ONCE(msg)                   \
    do                                        \
    {                                         \
        static bool _host_diag_done_ = false; \
        if (!_host_diag_done_)                \
        {                                     \
            _host_diag_done_ = true;          \
            Serial.print("[HostCore] ");      \
            Serial.println(msg);              \
        }                                     \
    } while (0)

#endif
