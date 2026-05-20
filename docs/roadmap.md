# host-arduino-core Roadmap

English | 日本語: [roadmap.ja.md](roadmap.ja.md)

What this core can and cannot do for host-side Arduino sketch testing. The
goal is to let sketch *logic* (parsing, state machines, protocol framing) be
verified on a developer machine without flashing real hardware.

Anything tied to physical I/O, vendor SDKs, or specialty radios is out of
scope and likely to stay that way — those can't be meaningfully emulated
without becoming a different project.

## Legend

- ✅ Implemented and exercised by tests under `tests/`
- 🟡 Stub — compiles and links, but the body does nothing meaningful
- 🔲 Planned / open for contribution
- ⛔ Out of scope — won't be implemented in this core

## Status

### Runtime / language

| API | Status | Notes |
|-----|--------|-------|
| `setup()` / `loop()` | ✅ | weak `main` in `cores/host/main.cpp` |
| `millis` / `micros` | ✅ | `std::chrono::steady_clock` |
| `delay` / `delayMicroseconds` | ✅ | `std::this_thread::sleep_for` |
| `yield` | ✅ | no-op |
| `min` / `max` / `constrain` / `map` | ✅ | header-only |
| `random` / `randomSeed` | ✅ | wraps `std::rand` |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | macros |

### Serial / Print

| API | Status | Notes |
|-----|--------|-------|
| `Serial` (UART facade) | ✅ | Exposed over a localhost TCP socket; see `Highlights` in README |
| `Print` (int / hex / bin / float / String / bool) | ✅ | matches Arduino formatting |
| `Stream` (timedRead / readBytes / setTimeout) | ✅ | |

### Filesystem

| API | Status | Notes |
|-----|--------|-------|
| `LittleFS` / `SPIFFS` / `FFat` / `SD` | ✅ | All backed by a directory next to the executable. No flash quotas, no formatting semantics |
| `File` (read / write / seek / size / openNextFile) | ✅ | wraps `<cstdio>` |

### Networking

| API | Status | Notes |
|-----|--------|-------|
| `IPAddress` | ✅ | full Arduino-compatible API |
| `UDP` abstract | ✅ | `cores/host/Udp.h` |
| `WiFiUDP` | ✅ | POSIX / Winsock backed; `SO_BROADCAST` enabled by default; `lastError()` exposes errno; rx buffer 65535 B |
| `WiFiUDP::beginMulticast` | 🔲 | base returns 0 (not joined). Needed for protocols like VBAN |
| `WiFi` facade | 🟡 | state-tracked stub (`begin` → `WL_CONNECTED`, `disconnect` → `WL_DISCONNECTED`). No real association, no scan |
| `Client` / `Server` abstract | 🔲 | needed before TCP impls |
| `WiFiClient` (TCP) | 🔲 | doable on POSIX sockets, ~250 LOC |
| `WiFiServer` (TCP) | 🔲 | doable on POSIX sockets, ~120 LOC |
| `WiFiClientSecure` (TLS) | ⛔ | would need mbedTLS/OpenSSL — out of scope here |
| `HTTPClient` / `WebServer` | ⛔ | belongs in a separate library on top of `Client`/`Server` |
| mDNS / DNSServer | 🔲 | low priority |

### Hardware I/O (intentionally stubbed)

| API | Status | Notes |
|-----|--------|-------|
| `pinMode` / `digitalWrite` / `digitalRead` | 🟡 | no-op stubs; `digitalRead` returns `0` |
| `analogRead` / `analogWrite` / attenuation / resolution | 🟡 | no-op stubs |
| `touchRead` | 🟡 | returns `0` |
| `tone` / `noTone` / `pulseIn` | 🟡 | no-op stubs |
| `attachInterrupt` / `detachInterrupt` | 🟡 | no-op stubs |
| `Wire` (I²C) | 🔲 | would only be a stub anyway; provide if a sketch refuses to compile without it |
| `SPI` | 🔲 | same as Wire |

These are stubbed so that real-hardware sketches at least link. Tests that
care about pin state should mock at the sketch layer.

### ESP-IDF / vendor extensions

| API | Status | Notes |
|-----|--------|-------|
| FreeRTOS (`xTaskCreate`, queues, semaphores) | ⛔ | out of scope; sketches that depend on RTOS scheduling should not be tested on host |
| `esp_log` / `log_*` macros | 🔲 | could route to `Serial` |
| `Preferences` (NVS) | 🔲 | could be backed by a file next to the executable |
| `EEPROM` | 🔲 | could be backed by a file |
| `Update` / OTA | ⛔ | meaningless on host |
| BLE / Classic Bluetooth | ⛔ | no plan |

## What "Planned" actually means

`🔲 Planned` items have no schedule. They are open for contribution and
will be picked up when a concrete sketch needs them. If you have such a
sketch, an issue describing the API surface it touches is the most useful
contribution.

## What's tested

The matrix above tracks API existence; the `tests/` directory tracks
behavioral coverage:

```
tests/
  runtime/  smoke, timing, print_api
  storage/  fs
  network/  udp_recv, udp_echo, udp_broadcast, wifi
```

Each leaf has a `.ino` + `sketch.yaml` + `test_*.py`. New tests prove a
square in the matrix goes green.
