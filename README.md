# host-arduino-core

English | 日本語: [README.ja.md](README.ja.md)

https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json

Minimal Arduino Core + Boards Manager package for building Arduino sketches as host executables and driving them from Arduino CLI based host-side tests such as `pytest-embedded-arduino-cli`.

This is not a practical Arduino-compatible board. It is a test target for pure sketch/library logic that does not require real hardware, flashing, or a serial port.

## Highlights

- Architecture `host`, automated-test FQBN `lang-ship:host:host`.
- Core files live in `cores/host` and provide a small `Arduino.h`, timing helpers, `Serial`, and a weak Arduino-style `main()`.
- `arduino-cli compile` builds a regular executable using the host-provided `gcc` / `g++` compatible toolchain.
- `arduino-cli upload` starts the executable in the background instead of flashing hardware.
- `arduino-cli monitor` is not supported.
- `Serial` is exposed over a localhost TCP connection for test automation.
- Startup connection data is printed to stdout and written next to the executable as `<executable>.host-arduino.json`.
- Releases are published as `host-arduino-core-<version>.zip` and indexed via `package_index.json`, served from GitHub Pages:
  `https://tanakamasayuki.github.io/host-arduino-core/package_index.json`

## API Support

The base assumption is ESP32-class boards, but the goal is to track the Arduino
Core API as closely as possible so sketches written against either surface can
be exercised on the host. "Source" indicates whether the API is part of the
Arduino Core standard or an ESP32 extension.

Legend:
- ✅ Implemented and exercised by tests under `tests/`
- 🟡 Stub — compiles and links, but the body does nothing meaningful
- 🔲 Planned / open for contribution
- ⛔ Out of scope — won't be implemented in this core

### Runtime / language

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `setup()` / `loop()` / weak `main` | ✅ | Arduino | `cores/host/main.cpp`. Four fixed points around the thunk reach an optional hook — see [Lifecycle Port](#lifecycle-port) |
| `millis` / `micros` | ✅ | Arduino | `std::chrono::steady_clock` by default, through the [Clock Port](#clock-port), so a test can substitute a virtual clock. 32-bit, so they wrap as on silicon |
| `delay` / `delayMicroseconds` | ✅ | Arduino | `std::this_thread::sleep_for` by default, through the [Clock Port](#clock-port). Override it and `delay(5000)` costs no wall-clock time; `delay`'s loop, `runtimePoll()`, and the shutdown check stay in the core |
| `yield` | ✅ | Arduino | `runtimePoll()` plus a zero-length wait through the [Clock Port](#clock-port), which is where a busy-waiting sketch gives a test driver its chance to run |
| `min` / `max` / `constrain` / `map` / `abs` | ✅ | Arduino | header-only |
| `random` / `randomSeed` | ✅ | Arduino | wraps `std::rand` |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | Arduino | macros |
| `String` (`WString.h`) | ✅ | Arduino | |

### Print / Stream / Serial

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `Print` (int / hex / bin / float / String / bool) | ✅ | Arduino | matches Arduino formatting |
| `Printable` | ✅ | Arduino | |
| `Stream` (`timedRead` / `readBytes` / `setTimeout` / `find` / `parseInt`) | ✅ | Arduino | the timeouts go through the [Clock Port](#clock-port), so they follow a virtual clock and a driver can answer from inside a blocking read |
| `HardwareSerial` / `Serial` | ✅ | Arduino | exposed over a localhost TCP socket |
| `Serial1` / `Serial2` | ✅ | ESP32 | `cores/host/HostUart.h`. Device-facing UARTs, separate from the console: both directions are in-memory queues a test drives — see [Device UARTs](#device-uarts). Not `HardwareSerial`, which stays an alias for the console class |

### Filesystem

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `FS` / `File` (read / write / seek / size / openNextFile) | ✅ | Arduino | wraps `<cstdio>` |
| `LittleFS` / `SPIFFS` / `FFat` / `SD` | ✅ | ESP32 | all backed by a directory next to the executable; no flash quotas / format semantics |
| `Preferences` (NVS) | ✅ | ESP32 | in-memory only (see ESP-IDF row for details) |
| `EEPROM` | 🔲 | Arduino | same approach as `Preferences` |

### Networking

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `IPAddress` | ✅ | Arduino | full Arduino-compatible API |
| `UDP` (abstract) | ✅ | Arduino | `cores/host/Udp.h` |
| `WiFiUDP` (unicast + broadcast) | ✅ | ESP32 | POSIX / Winsock backed; `SO_BROADCAST` enabled by default; `lastError()` exposes errno; rx buffer 65535 B. Requires `begin(0)` before any packet op (ESP32 is more lenient). Misuse emits `[HostCore]` hints over `Serial` (see `cores/host/HostDiag.h`) |
| `WiFiUDP::beginMulticast` | 🔲 | ESP32 | low priority — base returns 0 (not joined). Cross-platform multicast testing on the host (esp. Windows / WSL2) is fragile, so this is not actively pursued |
| `WiFi` facade (`begin` / `disconnect` / `status` / `localIP` / `SSID` / `RSSI` / `mode`) | 🟡 | ESP32 | state-tracked stub (`begin` → `WL_CONNECTED`, `disconnect` → `WL_DISCONNECTED`). No real association, no scan |
| `WiFi.scanNetworks` / `scanComplete` | 🔲 | ESP32 | stub returning 0 would be sufficient |
| `WiFi.softAP*` (real AP) | 🟡 | ESP32 | `softAPIP()` only; cannot become a real AP |
| `Client` / `Server` (abstract) | ✅ | Arduino | `cores/host/Client.h`, `cores/host/Server.h` |
| `WiFiClient` (TCP) | ✅ | Arduino / ESP32 | POSIX / Winsock backed; non-blocking socket; copy-shared via `shared_ptr` so `WiFiClient c = server.available();` works. `lastError()` exposes errno; misuse emits `[HostCore]` hints via `HostDiag` |
| `WiFiServer` (TCP) | ✅ | Arduino / ESP32 | POSIX / Winsock backed; `begin(port)` with `port=0` lets the OS pick an ephemeral port (`port()` returns the resolved value); `available()` / `accept()` are non-blocking |
| `WiFiClientSecure` (TLS) | ✅ | ESP32 | `cores/host/WiFiClientSecure.h` extends `WiFiClient` and uses OpenSSL when the `tls=openssl` board menu option is selected (verified on Linux `libssl-dev` 3.0.x and Windows MSYS2 UCRT64 `openssl` 3.5.2). When `tls=disabled` (the default), the class still compiles — `connect()` returns 0 and emits a `[HostCore]` hint, the build always succeeds. Certificate verification is always skipped on host (real-device test concern); `setCACert` / `setCertificate` / `setPrivateKey` / `setInsecure` / `loadCACert(Stream&,size_t)` are no-op shims. macOS is out of scope for the OpenSSL backend (needs additional `-I` / `-L` paths). A future mbedTLS source-build library is still planned as an alternative backend that bypasses the OS-package dependency |
| `HTTPClient` | ✅ | ESP32 | `cores/host/HTTPClient.h`. `begin(url)` auto-selects `WiFiClient` for `http://` and `WiFiClientSecure` for `https://` (the latter requires the `tls=openssl` board menu option; without it, `begin("https://…")` returns false and emits a `[HostCore]` hint). Supports `GET` / `POST` / `PUT` / `PATCH` / `sendRequest`, `addHeader`, `getString` (decodes both `Content-Length` and `Transfer-Encoding: chunked`), `getStream` / `getStreamPtr`, `getSize`, `getLocation`, `setTimeout`, `setUserAgent`, `setAuthorization`. Redirect following via `setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS / HTTPC_STRICT_FOLLOW_REDIRECTS / HTTPC_FORCE_FOLLOW_REDIRECTS)` + `setRedirectLimit(N)` (default disabled, limit 10): handles 301/302/303/307/308, absolute and root-relative `Location`, scheme transitions between `http` ⇄ `https` in internal-client mode, RFC-style POST→GET downgrade for 301/302/303 in FORCE mode. **User-added request headers (`addHeader` values) are dropped across redirects**, matching ESP32 Arduino HTTPClient v3.x — sketches that need persistent headers should set DISABLE mode, observe the 3xx, and re-issue manually. Connection strategy is `Connection: close` per request — no keep-alive. **Not implemented**: multipart, gzip, cookies, basic-auth helpers (use `addHeader("Authorization", ...)` directly) |
| `WebServer` / `AsyncWebServer` | ⛔ | ESP32 | same — separate library on top of `Server` |
| `ESPmDNS` / `DNSServer` | 🔲 | ESP32 | low priority |
| `Ping` / `NetworkInterface` | 🔲 | ESP32 | low priority |
| Ethernet (`ETH`) | 🔲 | ESP32 | on the host there is no real distinction from `WiFi` |

### Hardware I/O

Most of these are stubbed so that real-hardware sketches at least link.
The digital pins, the analog output (PWM / DAC), `SPI`, and `Wire` are
more than stubs: they form the [bus observation port](#bus-observation-port),
which lets a library keep its own device model and drive it from what the
sketch puts on the bus.

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `pinMode` / `digitalWrite` / `digitalRead` | ✅ | Arduino | pin state is held: `digitalRead` returns the last value written, `INPUT_PULLUP` reads HIGH. Every write is announced to an optional hook — see [Bus Observation Port](#bus-observation-port). No electrical behavior, no timing |
| `analogRead` / `analogReadMilliVolts` / `analogReadResolution` / `analogSetWidth` | ✅ | Arduino / ESP32 | reads what `HostArduino::setAnalogValue` / `setAnalogMilliVolts` injected, or what an analog read hook computes — the response direction of the [analog half](#bus-observation-port) of the port. Resolution is recorded, never applied: scaling the injected value would mean inventing a reference |
| `analogWrite` / `analogWriteFrequency` / `analogWriteResolution` | ✅ | Arduino / ESP32 | routed through LEDC, so an unattached pin is attached with the global defaults on first use exactly as arduino-esp32 does. Every call is announced to `HostArduino::setAnalogWriteHook` with the pin, channel, frequency, resolution, and duty |
| `ledcAttach` / `ledcAttachChannel` / `ledcWrite` / `ledcWriteChannel` / `ledcRead` / `ledcReadFreq` / `ledcChangeFrequency` / `ledcDetach` / `ledcWriteTone` / `ledcWriteNote` / `ledcOutputInvert` / `ledcFade*` | ✅ | ESP32 | per-pin state plus the same hook. Refusals match silicon (duty on an unattached pin, a re-attach, a resolution wider than 20 bits, a zero frequency) and report no event. 16 channels, classic-ESP32 numbering. Fades land on the target immediately — `max_fade_time_ms` is ignored and both endpoints are reported. The 2.x `ledcSetup` / `ledcAttachPin` spellings are **not** provided: arduino-esp32 3.x removed them, and `ledcWriteChannel` covers the channel-based write |
| `dacWrite` / `dacDisable` | ✅ | ESP32 | tracked in the same per-pin slot with no channel and no frequency, so one trace covers PWM and DAC. Any pin is accepted — which pins a board wires to a DAC is a variant detail the core does not carry |
| `tone` / `noTone` | ✅ | Arduino | on top of LEDC, one tone at a time as in arduino-esp32. A non-zero `duration` does not block: the tone and the silence are reported back to back, so a sketch shared with real silicon keeps the same call sequence without a wall-clock wait |
| `analogSetAttenuation` / `analogSetPinAttenuation` | 🟡 | ESP32 | accepted, no effect — there is no attenuator to configure. Inject the reading you want instead |
| `analogContinuous*` | 🔲 | ESP32 | not provided; no concrete sketch has needed it |
| `touchRead` / `touchAttachInterrupt` | 🟡 | ESP32 | returns `0` |
| `pulseIn` / `pulseInLong` | 🟡 | Arduino | no-op stubs |
| `attachInterrupt` / `detachInterrupt` | 🟡 | Arduino | no-op stubs |
| `Wire` (I²C) | ✅ | Arduino | bundled `Wire` library. Init succeeds and no device answers (`endTransmission()` → 2, `requestFrom()` → 0) until a library-side model registers transaction hooks. `begin()` / `end()` / `setPins` / `setClock` / `setTimeOut` reach a lifecycle hook, so bus setup lands in the same ordered trace as the traffic. `Wire1` also provided |
| `SPI` | ✅ | Arduino | bundled `SPI` library. Every transferred byte reaches a hook whose return value is what the sketch reads back on MISO; `SPISettings` is exposed to a transaction hook, and `begin()` / `end()` / the configuration setters to a lifecycle hook. Timing is not modelled |
| `Servo` | 🔲 | Arduino | no-op stub |

### ESP-IDF / vendor extensions

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| FreeRTOS (`xTaskCreate`, `vTaskDelay`, queues, semaphores, mutexes, notifications) | ✅ | ESP-IDF | backed by `std::thread` / `std::mutex` / `std::condition_variable`. Priority, core affinity, and stack size arguments are accepted and ignored. `portENTER_CRITICAL` collapses to a global recursive mutex. `vTaskDelete(NULL)` sets an exit flag and returns; the task function is expected to observe it and return — we cannot portably terminate another thread. Non-goals: priority-based scheduling, core pinning, `vTaskSuspend` immediacy, stack-overflow detection, real interrupt masking — sketches that rely on those should not be tested on host |
| `esp_log` / `ESP_LOG*` / `log_e` / `log_i` / `log_w` / `log_d` / `log_v` macros | ✅ | ESP-IDF | host stubs only — `CORE_DEBUG_LEVEL` is fixed at `ARDUHAL_LOG_LEVEL_NONE` (0) and all macros expand to a `(void)sizeof(...)` discard (no output, no unused-variable warnings). `esp_log_level_set` is a no-op; `esp_log_level_get` always returns `ESP_LOG_NONE`. Rationale: mixing log output into `dut.expect` streams hurts test readability, and Arduino's own convention is `Serial.print` for diagnostics |
| `esp_timer` | ✅ | ESP-IDF | `esp_timer_get_time()` returns µs since first call. Full `create` / `start_once` / `start_periodic` / `stop` / `delete` / `is_active` surface is backed by one `std::thread` per timer with a `condition_variable` for scheduling. No real-time guarantees |
| `esp_random` / `esp_fill_random` | ✅ | ESP-IDF | backed by `std::mt19937` seeded from `std::random_device`. Non-cryptographic — same usage class as the silicon's hardware RNG in Arduino sketches |
| `Preferences` (NVS) | ✅ | ESP32 | in-memory only. Full put/get surface for scalar, string, and bytes types. Reading a missing key (or a key stored with a different type) returns the supplied default. Values do not persist across sketch exits — boot-crossing behavior must be verified via interop against real silicon |
| Raw `nvs_flash_*` API | ⛔ | ESP-IDF | use `Preferences` instead |
| `Update` / OTA | ⛔ | ESP32 | meaningless on host |
| BLE / Classic Bluetooth | ⛔ | ESP32 | no plan |
| ESP-NOW | 🔲 | ESP32 | could be faked over UDP broadcast, but low priority |
| Camera (`esp_camera`) | ⛔ | ESP32 | out of scope |

### Graphics / Display

| API | Status | Notes |
|-----|--------|-------|
| M5GFX / LovyanGFX headless backend | ✅ | enabled by selecting `mode=lgfx` on the FQBN (e.g. `lang-ship:host:host:mode=lgfx`). Sets `SDL_VIDEODRIVER=dummy` in `main()`, wires `Panel_sdl::main` to drive ordinary `setup()`/`loop()` from a worker thread, and leaves `ARDUINO` undefined so M5GFX/LovyanGFX picks its SDL backend in `device.hpp`. Sketches can `Serial.print()` over the TCP runtime as usual and call `gfx.createPng()` to capture a frame for assertion (see `tests/graphics/lgfx_smoke`). Linking pulls `-lSDL2` automatically; on Linux install `libsdl2-dev`. Requires either the M5GFX or LovyanGFX library to be in scope (the core forward-declares `lgfx::v1::Panel_sdl::main` and resolves it at final link) |
| SDL2 manual display board | ✅ | enabled by selecting `lang-ship:host:display`. `upload` opens a foreground SDL2 window for manual checks. It shares `cores/host` with the `Host` board, so the Core API surface stays identical. Per-device board IDs such as M5Stack / Core2 / CoreS3 are not added; choose device, scale, and rotation through the `display` board menus or example `sketch.yaml` profiles. It does not use the TCP runtime; `Serial` and M5Unified `M5_LOG*` output go to stdout |
| TFT_eSPI | 🔲 | no plan yet |

### What "Planned" actually means

`🔲 Planned` items have no schedule. They are open for contribution and
will be picked up when a concrete sketch needs them. If you have such a
sketch, an issue describing the API surface it touches is the most useful
contribution.

### What's tested

The matrix above tracks API existence; the `tests/` directory tracks
behavioral coverage:

```
tests/
  runtime/  smoke, timing, print_api, esp_log, lifecycle_hook,
            clock_hook, uart_buffer, gpio_hook, analog_hook, spi_hook,
            wire_hook, bus_trace, esp_random, esp_timer,
            freertos_mutex, freertos_notify, freertos_queue,
            freertos_task
  storage/  fs, preferences
  network/  udp_recv, udp_echo, udp_broadcast, udp_no_begin, wifi,
            tcp_echo, tcp_client, tls_openssl, tls_secure_connect,
            http_get, https_get, http_redirect
  interop/  smoke, wifi_connect, https_get, http_redirect, http_chunked
```

`runtime/`, `storage/`, and `network/` are host-only. `interop/` sketches
build and run on **both** the host runtime and real ESP32 silicon — the
sketch source is identical across the two profiles (no `#ifdef`). Run
them against ESP32 with:

```bash
cd tests
uv run --env-file .env pytest --profile esp32 interop/
```

The `.env` file holds `TEST_WIFI_SSID`, `TEST_WIFI_PASSWORD`, and the
ESP32 serial port. Tests that need build-time injection (e.g.
`wifi_connect`) declare the mapping in their own `build_config.toml`
(`TEST_WIFI_SSID = "WIFI_SSID"` → `-DWIFI_SSID="..."`). Sketches use
`Serial.begin(115200); delay(5000);` to satisfy the real-silicon
boot-up settling window — `delay()` on the host runtime is just
`std::this_thread::sleep_for`, so the same source compiles and runs
cleanly on both targets.

Each leaf has a `.ino` + `sketch.yaml` + `test_*.py`. New tests prove a
square in the matrix goes green.

## Bus Observation Port

Peripherals are deliberately not modelled by this core. The code that knows
a device's protocol is the library that drives that device — an ST7789
init sequence belongs to a display library, the SD command set to an SD
library — so a core that started collecting device models would never stop
growing. What the core provides instead is a place to watch a bus from and
to answer on. The device model stays on the library side.

```text
sketch ──SPI.transfer()──▶ core SPI ─────┐
sketch ──ledcWrite()─────▶ core LEDC ────┤──▶ observation / response hook
sketch ──digitalWrite()──▶ core GPIO ────┘         │
                        (write announced)          ▼   library-side model
                                          e.g. an ST7789 decoder
                                                   │
                                                   ▼
                                          virtual GRAM → PPM / PNG
```

| Bus | Declared in | Hooks |
|-----|-------------|-------|
| GPIO | `cores/host/HostBus.h` (always available) | `HostArduino::setPinWriteHook` / `setPinReadHook` / `setPinModeHook` |
| Analog / PWM (`analogWrite`, `ledc*`, `dacWrite`, `tone`) | `cores/host/HostBus.h` (always available) | `HostArduino::setAnalogWriteHook` / `setAnalogReadHook` |
| SPI | bundled `SPI` library | `SPI.setTransferHook` / `SPI.setTransactionHook` / `SPI.setLifecycleHook` |
| I²C | bundled `Wire` library | `Wire.setWriteHook` / `Wire.setReadHook` / `Wire.setLifecycleHook` |

Guard library-side code with `HOST_ARDUINO`, which `platform.txt` always
defines for both boards.

**One slot per hook kind.** Registering replaces whatever was registered
before — there is no chain, because a chain needs allocation on a path that
runs millions of times per frame. One library at a time can therefore own a
bus in a given sketch: a model that shares the bus with itself dispatches on
the pin number or the address, but two libraries cannot both observe the same
bus in one test. Pass `nullptr` (or `clearPinHooks()` / `clearHooks()`) to
release a slot, which is how a test hands the bus from one model to the next.

### GPIO — the one that covers bit-banged transports

This is the half that matters most, because a bit-banged transport never
goes through a bus class: soft SPI, soft I²C, WS2812, and IR pulses all
just call `digitalWrite`.

- Every `digitalWrite` is announced to the write hook, same-value writes
  included — edge detection is the model's job.
- Each pin holds the last value written; `digitalRead` returns it.
- `pinMode(pin, INPUT_PULLUP)` seeds the held value HIGH (`INPUT_PULLDOWN`
  LOW), so a released open-drain line reads back the way it would on real
  silicon. `INPUT` leaves the held value alone — a floating pin has no
  defined level.
- `HostArduino::setPinValue(pin, level)` injects an input level: the
  response direction for GPIO. `setPinReadHook` is there for levels a model
  has to compute at read time.
- Pins 0–255 are tracked. Writes outside that range are dropped without a
  hook call, reads return 0.

```cpp
#include <Arduino.h>

// Stand-in for what a display library would own: reassemble bytes from
// SCK rising edges, tell command from data by reading the DC line.
void onPinWrite(uint8_t pin, uint8_t value, void *user)
{
    auto *model = static_cast<PanelModel *>(user);
    if (pin != PIN_SCK || !value) return;
    model->shiftIn(digitalRead(PIN_MOSI));
    if (model->byteComplete()) {
        model->feedByte(digitalRead(PIN_DC) == LOW);   // LOW = command
    }
}

HostArduino::setPinWriteHook(onPinWrite, &model);
```

Cost, measured on one 240x240 16bpp frame pushed out over bit-banged SPI
(2.76 million `digitalWrite` calls): 1.1 ms with no hook, 7.2 ms with one.
The write path is inline and the hook is a plain function pointer, not a
`std::function`.

### Analog / PWM — the backlight half

`analogWrite`, the `ledc*` family, `dacWrite`, and `tone` all end up driving
one pin's analog output, and all of them report to one hook. A no-op stub
would let a display driver link; it would also make its backlight
initialization invisible, which is the one part of a panel bring-up whose
*ordering* matters most — configured dark, init sequence, then lit.

One hook rather than one slot per call, because what a test asserts is an
ordered sequence: a single stream can be compared against a golden list,
four streams would have to be re-interleaved first. The events are coarser
than the API, so a trace records what happened to the pin rather than which
spelling the caller reached for:

| Event | Reported by |
|-------|-------------|
| `kAnalogAttach` | `ledcAttach`, `ledcAttachChannel`, and the implicit attach inside `analogWrite` / `tone` |
| `kAnalogWrite` | `ledcWrite`, `ledcWriteChannel`, `analogWrite`, and both endpoints of `ledcFade*` |
| `kAnalogConfig` | `ledcChangeFrequency`, `ledcOutputInvert`, `analogWriteFrequency`, `analogWriteResolution` |
| `kAnalogTone` | `ledcWriteTone`, `ledcWriteNote`, `tone` |
| `kAnalogDetach` | `ledcDetach`, `noTone`, `dacDisable` |
| `kAnalogDac` | `dacWrite` |

```cpp
#include <Arduino.h>

void onBacklight(HostArduino::AnalogWriteEvent event, const HostArduino::AnalogOut &out, void *user)
{
    // out.pin / out.channel / out.frequency / out.resolution / out.duty
    static_cast<Trace *>(user)->add(event, out);
}

HostArduino::setAnalogWriteHook(onBacklight, &trace);
ledcAttach(PIN_BL, 5000, 8);   // kAnalogAttach, channel 0, 5000 Hz, 8 bits
ledcWrite(PIN_BL, 0);          // kAnalogWrite, duty 0 — dark during init
ledcWrite(PIN_BL, 200);        // kAnalogWrite, duty 200 — and now lit
```

`HostArduino::analogOut(pin)` reports the same state without a hook, which
is what to assert against when only the end state matters. Prefer it over
`ledcReadFreq(pin)`: that one is faithful to arduino-esp32 and reads 0 Hz
while the duty is 0, which hides what was configured.

- A call silicon would refuse is refused — a duty write to an unattached
  pin, a re-attach, a resolution wider than `HostArduino::kLedcMaxResolution`
  (20), a zero frequency — and reports no event, so a trace never shows work
  a real board would not have done.
- `ledcAttach` hands out the lowest free channel, the same one arduino-esp32
  would have picked, so the channel in the trace is the channel the sketch
  would have got.
- The read direction: `HostArduino::setAnalogValue(pin, raw)` and
  `setAnalogMilliVolts(pin, mv)` inject what `analogRead` /
  `analogReadMilliVolts` return, and `setAnalogReadHook` is there for a
  reading a model has to compute. The two quantities are injected
  separately on purpose — deriving one from the other needs an attenuation
  and Vref model the core does not have.
- Not modelled: waveforms, timing, timer sharing. Nothing is emitted on the
  pin, `digitalRead` does not see a PWM signal, and a duty of 128/255 does
  not make anything half-bright. Fades land on the target immediately.

### SPI

`SPI.transfer()` hands every byte to the transfer hook and returns what the
hook returned, so one hook covers both watching the conversation and
answering on MISO. With no hook registered a transfer reads `0xFF` — an
idle bus with nothing driving it.

```cpp
#include <SPI.h>

uint8_t onByte(uint8_t out, void *user)
{
    static_cast<MyPanelModel *>(user)->feedByte(out);
    return 0xFF;                       // write-only device
}

void onTransaction(bool active, const SPISettings &s, void *user)
{
    if (active) Serial.printf("%u Hz, order %u, mode %u\n", s.clock(), s.bitOrder(), s.dataMode());
}

SPI.setTransferHook(onByte, &model);
SPI.setTransactionHook(onTransaction);
```

`SPISettings` exposes `clock()` / `bitOrder()` / `dataMode()` for reading, and
keeps the underscored `_clock` / `_bitOrder` / `_dataMode` fields public
because that is the spelling arduino-esp32 sketches use. `SPI.settings()` /
`inTransaction()` / `transferCount()` / `sck()` / `miso()` / `mosi()` / `ss()`
let a test assert the wiring and the traffic without a hook at all. The hook is byte-granular: bit order is reported
through `SPISettings`, never applied to the byte, because there is no wire
to serialize onto. Clock rates are recorded, never honored.

`SPI.setLifecycleHook` covers the bus itself — `SPIClass::kBegin`,
`kEnd`, and `kConfig` for `setFrequency` / `setBitOrder` / `setDataMode` /
`setClockDivider` / `setHwCs`, the transaction-free spelling a driver uses
when it owns the bus outright. The pins were always readable afterwards;
the hook is what says *when* the bus came up relative to the reset pulse
and the backlight next to it.

### I²C

`Wire` initializes successfully and finds nothing on the bus — the shape a
scan loop expects for an empty address. Its hooks are transaction-granular
rather than byte-granular, because that is the level an I²C device model
works at: an address, a payload, a stop condition.

```cpp
#include <Wire.h>

uint8_t onWrite(uint8_t addr, const uint8_t *data, size_t len, bool stop, void *user)
{
    if (addr != 0x68) return 2;        // address NACK — nobody home
    static_cast<MySensor *>(user)->command(data, len);
    return 0;                          // ACK
}

size_t onRead(uint8_t addr, uint8_t *data, size_t len, bool stop, void *user)
{
    return addr == 0x68 ? static_cast<MySensor *>(user)->fill(data, len) : 0;
}

Wire.setWriteHook(onWrite, &sensor);
Wire.setReadHook(onRead, &sensor);
```

`Wire.setLifecycleHook` covers the bus itself — `TwoWire::kBegin`, `kEnd`,
`kSetPins`, `kSetClock`, and `kSetTimeout`. `sda()` / `scl()` / `getClock()`
could always be read afterwards, but only the hook says *when* the bus was
brought up relative to everything else a driver did. The hook receives the
whole `TwoWire`, so `busNum()` tells `Wire` from `Wire1` when one model
watches both.

Not modelled: clock stretching, arbitration, bus timing, and the slave role
(`onReceive` / `onRequest` are accepted and never called).

### Golden traces

Registering every hook and appending one line per event to a single buffer
gives an ordered record of a driver's whole startup — I²C up, reset pulse,
SPI up, backlight configured dark, commands, backlight lit, touch probed —
that a test can compare against a golden list line for line:

```text
i2c.begin sda=21 scl=22 clock=100000
gpio.mode pin=33 mode=1
gpio.write pin=33 value=0
gpio.write pin=33 value=1
spi.begin sck=18 mosi=23 cs=5
pwm.attach pin=38 ch=0 f=5000 r=8
pwm.write pin=38 duty=0
spi.txn active=1 clock=40000000 mode=0
spi.byte 01 cs=0
...
pwm.write pin=38 duty=200
```

A step moving relative to its neighbours fails that comparison even though
every end-state assertion still passes, which is exactly the class of
display-driver bug an end-state test misses. Record from the hooks and print
once at the end rather than printing from inside them, so the trace stays
independent of whatever else the sketch writes to `Serial`.
`tests/runtime/bus_trace` is the worked example.

### Threading, and what is deliberately absent

Hooks run synchronously on the thread that called into the bus, and the pin
state is a plain array with no locking. With `mode=lgfx` (and on the
`display` board) `setup()` / `loop()` run on a worker thread, and FreeRTOS
tasks are `std::thread` — so register hooks before starting tasks, and keep
one bus to one thread.

- **No device models in the core.** No SD card, no LCD. Those belong to the
  libraries that speak their protocols.
- **No timestamps in hook signatures.** `micros()` is already available and
  monotonic; a hook that cares calls it, one that does not pays nothing.
- **No framebuffer-to-window API.** The SDL2 window on the `display` board
  belongs to LovyanGFX's `Panel_sdl`, not to the core — `cores/host/main.cpp`
  only forward-declares its entry point. A model that wants to be looked at
  can write PPM / PNG next to the executable, or push its pixels into an
  `LGFX_Sprite`.

Worked examples: `libraries/Host/examples/Plane/BusObserve`, and the tests
in `tests/runtime/gpio_hook`, `tests/runtime/analog_hook`,
`tests/runtime/spi_hook`, `tests/runtime/wire_hook`, and
`tests/runtime/bus_trace`.

## Extension Ports

The bus observation port above lets a library watch what a sketch puts on
a bus. Three smaller ports cover the rest of what a test harness needs to
sit under a sketch rather than beside it: where to run, what time it is,
and something to talk to. Each has one slot and one user — multiplexing
between several interested parties belongs to whatever layer sits above,
which is also why none of them replaced anything: the existing bus hooks
are untouched.

### Lifecycle Port

`cores/host/HostLifecycle.h`. Four fixed points around the Arduino thunk:

```text
runtimeStart()
  kPreSetup          once — arrange the fixture; Serial already works
  setup()
  kPostSetup         once — dump the state setup() left behind
  loop {
    kPreLoop         every iteration — run what the sketch is about to see
    loop()
    kPostLoop        every iteration — close the iteration out
    runtimePoll()
  }
```

```cpp
void onPhase(HostArduino::LifecyclePhase phase, void *user)
{
    if (phase == HostArduino::kPostLoop) advanceMyClock();
}

HostArduino::setLifecycleHook(onPhase, &harness);
```

**`kPostLoop` runs before `runtimePoll()`, not after.** External input is
therefore always taken in after the iteration has been closed out and is
stamped as belonging to the *next* one. With the other order, bytes picked
up by `runtimePoll()` would land in the iteration the sketch had already
finished running, and a trace would show the sketch alongside input it
never had a chance to see.

One hook with a phase argument rather than four slots, for the same reason
the analog half of the bus port has one: the four points are an ordered
sequence, and a driver that wants them as one stream should not have to
stitch four streams back together.

`HostArduino::loopCount()` counts iterations, incremented at `kPostLoop`
before the hook runs — so a driver reading it from `kPostLoop` sees the
iteration it is closing out. Register from a global constructor to catch
`kPreSetup`; registering from `setup()` has already missed it.

Both thunks announce the same four points: the plain host runtime and the
SDL one used by `mode=lgfx` and the `display` board. The SDL thunk runs on
a worker thread, so a driver that registers from `main` gets its hook on
another thread there.

### Clock Port

`cores/host/HostClock.h`. Two functions — what time is it, and wait a
slice:

```cpp
uint64_t virtualNow(void *user)                 { return c(user)->micros; }
void     virtualWait(uint32_t us, void *user)   { c(user)->micros += us; }

HostArduino::setClockHooks(virtualNow, virtualWait, &clock);
```

Every sketch-facing time function goes through it: `millis`, `micros`,
`delay`, `delayMicroseconds`, `yield`, and the `Stream` timeouts that
`readBytes` / `find` / `parseInt` are built on.

**Why the wait and not just the clock.** A driver that only replaced "what
time is it" would still sit through every `delay(1000)` in real seconds,
because the waiting happens somewhere else. Handing over the wait as well
is what makes the three useful states one mechanism:

| Installed | Result |
|-----------|--------|
| nothing | the real monotonic clock, `delay` really sleeps — unchanged from before this port existed |
| wait only | real time, plus your code on every 1 ms slice of every wait. The "tick callback" shape |
| both | virtual time: `delay(5000)` returns at host speed with the clock 5000 ms further on |

Installing **only** `now` is the one combination to avoid: the deadline
would be virtual while the wait stayed real, so `delay`'s loop would spin
without the clock ever reaching it.

**What stays in the core.** `delay`'s loop, `runtimePoll()`, and the
shutdown check are not overridable, so a driver cannot forget them:

```cpp
deadline = clockNowMicros() + ms * 1000;
while (!runtimeShouldStop() && clockNowMicros() < deadline) {
    runtimePoll();
    clockWaitMicros(1000);
}
```

`clockRealNowMicros()` / `clockRealWaitMicros()` reach the real clock
whatever is installed — a tick-shaped hook calls the latter to keep
sleeping for real, and a test measures its own wall-clock cost with the
former.

`yield()` is not a timed wait but goes through the port as a zero-length
one, because it is the one place a busy-waiting sketch offers the host a
chance to run. A sketch that spins on `while (millis() - t0 < 1000);` with
no `delay` and no `yield` inside will not terminate under a virtual clock
that only advances elsewhere; advancing a little on each reading is the
usual answer, and it belongs to the driver.

### Timeouts that stay on real time

The clock port does not cover every wait in the core. A test written
against a virtual clock needs to know which readings will not agree with
it, so here is the complete list.

**Deliberately outside the port** — these are self-consistent (real
deadline, real wait), so they always expire; they just cost wall-clock
time and disagree with a virtual `millis()`:

| What | Where | Note |
|------|-------|------|
| `vTaskDelay` / `vTaskDelayUntil` / `xTaskGetTickCount` | `cores/host/freertos/FreeRTOS.h` | a FreeRTOS task's own clock. `xTaskGetTickCount()` will not match `millis()` under a virtual clock |
| `xQueueSend` / `xQueueReceive` / `xSemaphoreTake` / `ulTaskNotifyTake` timeouts | `cores/host/freertos/FreeRTOS.h` | `condition_variable::wait_for`, so they wake early on notify — a two-condition wait a single "wait a slice" cannot express |
| `esp_timer` firing, and `esp_timer_get_time()` | `cores/host/esp_timer.h` | one thread per timer, waiting on a condition variable |
| `WiFiClientSecure` handshake budget (5 s) | `cores/host/WiFiClientSecure.h` | drives `SSL_connect` through `select()` |

These are the multi-thread cases, where "who advances the clock" needs an
answer before they can be virtualized. They are open for contribution;
until then, a sketch that mixes `millis()` with `xTaskGetTickCount()`
under a virtual clock will see the two diverge.

**Permanently on real time** — process startup and sockets, in
`cores/host/HostRuntime.cpp`: the TCP accept wait, `waitForClient`,
`HOST_ARDUINO_START_DELAY_MS`, the launcher's port-file wait, and the
send retry. Virtualizing these would stall the runtime before the test
harness had connected over a real socket, and nothing would start.

`HTTPClient`'s read timeout is **not** on this list: it takes its deadline
from `millis()` and waits with `delay(1)`, so it follows a virtual clock
correctly.

### Device UARTs

`cores/host/HostUart.h`. `Serial` is the console — it goes out over the
TCP runtime (or stdout on the `display` board) and is how a test reads
what the sketch printed. `Serial1` and `Serial2` are the other kind of
UART, the one a device hangs off, so they are not wired to anything
outside the process:

```text
sketch --write()--> tx queue --readTx()--> driver
sketch <--read()--- rx queue <--pushRx()-- driver
```

```cpp
Serial1.begin(9600, SERIAL_8N1, 16, 17);

// in the driver:
const String sent = Serial1.readTxString();
if (sent.startsWith("AT")) Serial1.pushRx("OK\r\n");
```

Because nothing else consumes either queue, a test owns the whole
conversation. `begun()` / `baudRate()` / `config()` / `rxPin()` /
`txPin()` report what `begin()` was given; `txTotal()` / `rxTotal()` count
traffic without decoding it; `txOverflowed()` / `rxOverflowed()` are
sticky flags set when a queue had to drop bytes, so a test checks once at
the end rather than after every push. Queues default to 1 KB each and
`setRxBufferSize` / `setTxBufferSize` change that.

**Answering inside one iteration.** A sketch that writes a command and
reads the reply before returning from `loop()` — every AT-command driver
does this — cannot be served from `kPreLoop`, because the reply would
arrive an iteration too late. It is serviceable through the clock port
instead: `Stream::readBytes` waits via `clockWaitMicros`, so a driver that
has overridden the wait can drain tx and push rx from inside the sketch's
own blocking read. `tests/runtime/uart_buffer` does it both ways.

**Not `HardwareSerial`.** On real silicon `Serial`, `Serial1`, and USB CDC
are all one class, but that class describes a peripheral this core does
not have, and taking a `HardwareSerial&` is a poor way for a library to
ask for "somewhere to talk" — `Stream&` says it without the baggage. So
`HostUart` derives from `Stream` and nothing else, and `HardwareSerial`
stays what it was, an alias for the console class. A library that insists
on `HardwareSerial&` will not accept `Serial1` here.

Not modelled: baud timing (bytes appear the instant they are written),
framing, parity, break detection, flow control.

Worked examples: `tests/runtime/lifecycle_hook`, `tests/runtime/clock_hook`,
and `tests/runtime/uart_buffer`.

## Boards

The standard boards are `Host` for automated tests and `Host Display` for
manual SDL2 display checks.

```text
lang-ship:host:host
```

- Package: `lang-ship`
- Architecture: `host`
- Board ID: `host`
- Board name: `Host`
- Core directory: `cores/host`

`Host` is the CI / pytest / headless board. `upload` starts the executable in
the background, and `Serial` is controlled by the test side through the TCP
runtime. If no TCP client connects within the timeout, or if the established
TCP connection is closed, the process exits so failed or interrupted tests do
not leave stale child processes behind.

For automated graphics tests, use the existing `mode=lgfx` menu:

```text
lang-ship:host:host:mode=lgfx
```

This mode uses SDL2's dummy video driver and is intended for PNG capture and
assertions from tests.

For manual display checks, use `Host Display`:

```text
lang-ship:host:display
```

`Host Display` uses the same `cores/host` implementation as `Host`, but opens
an SDL2 window on `upload`. It does not use the TCP runtime: there is no TCP
connection wait, no post-connect settle delay, no connection-info file, and no
TCP-disconnect shutdown. The sketch enters `setup()` / `loop()` immediately,
`Serial` output is written to stdout, and the process exits when the SDL2
window is closed, the sketch exits, or the user terminates the process.

For LovyanGFX sketches that need an explicit display size, select a target
from the `Display Board` menu. The menu defines both `M5GFX_BOARD` and the
generic `HOST_DISPLAY_WIDTH` / `HOST_DISPLAY_HEIGHT` values, so LovyanGFX can
use the same board choice.

```bash
arduino-cli upload -p NONE \
  --fqbn 'lang-ship:host:display:m5gfx_board=board_M5Stack,m5gfx_scale=x2,m5gfx_rotation=r0' \
  libraries/Host/examples/SDL2/HostDisplayLovyanGFX
```

This starts the example as `M5Stack (320x240)` at 2x scale and rotation 0.
For M5Unified, use `libraries/Host/examples/SDL2/HostDisplayM5Unified` with
the same FQBN.

M5Unified `M5_LOGE()` / `M5_LOGW()` / `M5_LOGI()` / `M5_LOGD()` /
`M5_LOGV()` expand to `M5.Log`, not ESP32 `ESP_LOG*`. In PC / SDL2 builds,
M5Unified prints those logs to stdout, so manual checks keep the SDL2 window
and log console separate.

## Repository Layout

- `platform.txt` / `boards.txt`: Arduino platform metadata copied into the release ZIP.
- `cores/host/Arduino.h`: minimal Arduino-facing API surface.
- `cores/host/HostRuntime.{h,cpp}`: host runtime, TCP-backed `Serial`, process launcher, and connection-info file handling.
- `cores/host/HostBus.{h,cpp}`: GPIO pin state, analog output (PWM / DAC) state, and the bus observation hooks (see [Bus Observation Port](#bus-observation-port)).
- `cores/host/HostLifecycle.{h,cpp}`: the four points around the Arduino thunk (see [Lifecycle Port](#lifecycle-port)).
- `cores/host/HostClock.{h,cpp}`: the one place the core reads the clock and the one place it waits (see [Clock Port](#clock-port)).
- `cores/host/HostUart.{h,cpp}`: `Serial1` / `Serial2`, the device-facing UARTs (see [Device UARTs](#device-uarts)).
- `libraries/SPI/`, `libraries/Wire/`: bundled `SPI` / `Wire` with the SPI and I²C halves of the same port.
- `cores/host/main.cpp`: weak `main()` that calls `setup()` once and then `loop()` until the runtime requests shutdown.
- `scripts/bump_version.py`: updates the `version=` entry in `platform.txt` and the host platform versions in `libraries/Host/examples/*/*/sketch.yaml`.
- `scripts/build_package.py`: creates `package/host-arduino-core/`, produces the ZIP, computes SHA-256, and updates `package_index.json`.
- `scripts/prepare_release.py`: moves `CHANGELOG.md` unreleased entries into the release version section.
- `.github/workflows/release.yml`: builds/releases the package on tag push or manual dispatch, publishes `package/` to `gh-pages`, and attaches assets to GitHub Releases.
- `CHANGELOG.md`: release notes in English and Japanese. The release workflow uses the matching version section as the GitHub Release body.
- `docs/requirements.ja.md`: requirements document.
- `libraries/Host/examples/`: ready-to-open sketches shipped in the release ZIP (e.g. `TLSProbe` for verifying the `tls=openssl` board menu option on each OS).
- `package_index.json`: checked-in Boards Manager index, updated by the release workflow.

## Prerequisites

This package does not install compilers, linkers, or other build tools through Boards Manager. Install a `gcc` / `g++` compatible host toolchain first.

- Windows: install MSYS2 and add the MinGW/UCRT toolchain directory to `PATH`.
  ```powershell
  winget install MSYS2.MSYS2
  ```
  Then install a GCC toolchain from MSYS2, for example UCRT64:
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-gcc
  ```
  Add `C:\msys64\ucrt64\bin` to `PATH`, open a new terminal, and check `g++ --version`.

- Linux:
  ```bash
  sudo apt update
  sudo apt install build-essential
  ```

- macOS:
  ```bash
  xcode-select --install
  ```

On macOS, `g++` is often Apple clang behind a GCC-compatible command name. That is acceptable for this core.

### Optional: OpenSSL (for TLS / HTTPS support)

Only needed if you plan to select the `tls=openssl` board menu option
(used by `WiFiClientSecure` and `HTTPClient` against `https://` URLs).
Skip this step if your sketches stay on plain TCP / UDP / HTTP.

- Linux (Debian / Ubuntu):
  ```bash
  sudo apt update
  sudo apt install libssl-dev
  ```

- Linux (Fedora / RHEL / Rocky):
  ```bash
  sudo dnf install openssl-devel
  ```

- Windows (MSYS2 UCRT64):
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-openssl
  ```

- macOS: **not currently supported** for the `tls=openssl` board option.
  The link recipe doesn't add the Homebrew-specific `-I` / `-L` paths
  yet. Tracked for a future revision; in the meantime, build `tls=disabled`
  sketches on macOS.

After installing the dev package, no further configuration is needed —
the headers and libraries land in the default include / library search
paths of the toolchain. Select **Tools → TLS → OpenSSL** in the Arduino
IDE (or set `tls=openssl` on the FQBN, e.g.
`lang-ship:host:host:tls=openssl`) to opt in.

Verify the link wiring with the included `TLSProbe` example sketch —
it prints `PROBE_RESULT=PASS` when OpenSSL is reachable and reports
`PROBE_RESULT=FAIL` (with a hint) otherwise.

### Optional: SDL2 (for headless LovyanGFX / M5GFX rendering)

Only needed if you plan to select the `mode=lgfx` board menu option
(used to run M5GFX / LovyanGFX / M5Unified sketches off-screen on the
host PC, e.g. for CI-driven layout tests). Skip this step if your
sketches don't touch graphics.

- Linux (Debian / Ubuntu):
  ```bash
  sudo apt update
  sudo apt install libsdl2-dev
  ```

- Linux (Fedora / RHEL / Rocky):
  ```bash
  sudo dnf install SDL2-devel
  ```

- Windows (MSYS2 UCRT64):
  ```bash
  C:\msys64\usr\bin\pacman -S mingw-w64-ucrt-x86_64-SDL2
  ```

- macOS (Homebrew):
  ```bash
  brew install sdl2
  ```
  The link recipe does not yet add Homebrew-specific `-I` / `-L` paths,
  so macOS `mode=lgfx` builds are not officially verified — same caveat
  as `tls=openssl`.

After installing the dev package, no further configuration is needed —
`-lSDL2` is injected by the `mode=lgfx` menu option and the standard
search paths cover the rest. Select **Tools → Mode → LovyanGFX /
M5GFX headless** in the Arduino IDE (or set `mode=lgfx` on the FQBN,
e.g. `lang-ship:host:host:mode=lgfx`) to opt in. SDL2 always runs with
`SDL_VIDEODRIVER=dummy` in this mode, so no window appears — output
is captured via `gfx.createPng()` to disk.

Verify the wiring with `tests/graphics/lovyangfx_smoke` (or
`m5gfx_smoke` / `m5unified_smoke`); each one renders a few sample
panels and writes them as PNG files into the test directory's
`output/`.

## Arduino CLI Workflow

1. Register the Boards Manager index:

   ```bash
   arduino-cli config add board_manager.additional_urls https://tanakamasayuki.github.io/host-arduino-core/package_index.json
   arduino-cli core update-index
   arduino-cli core install lang-ship:host
   ```

2. Create a smoke-test sketch:

   ```bash
   mkdir -p /tmp/HostSmoke
   cat >/tmp/HostSmoke/HostSmoke.ino <<'EOF'
   #include <Arduino.h>

   void setup() {
     Serial.println("boot");
   }

   void loop() {
     if (Serial.available()) {
       int c = Serial.read();
       Serial.print("rx:");
       Serial.println((char)c);
     }
     delay(10);
   }
   EOF
   ```

3. Compile:

   ```bash
   cd /tmp/HostSmoke
   arduino-cli compile --fqbn lang-ship:host:host --build-path .build --clean
   ```

4. Upload, which starts the executable in the background:

   ```bash
   arduino-cli upload --fqbn lang-ship:host:host --input-dir .build
   ```

   Expected output includes:

   ```text
   HOST_ARDUINO_PORT=xxxxx
   HOST_ARDUINO_INFO=/tmp/HostSmoke/.build/HostSmoke.ino.out.host-arduino.json
   ```

5. Connect to the TCP-backed `Serial` endpoint:

   ```bash
   python3 - <<'PY'
   import json, socket, time

   info_path = "/tmp/HostSmoke/.build/HostSmoke.ino.out.host-arduino.json"
   with open(info_path) as f:
       info = json.load(f)

   s = socket.create_connection(("127.0.0.1", info["port"]), timeout=2)
   s.settimeout(2)
   print(s.recv(1024))
   s.sendall(b"A")
   time.sleep(0.1)
   print(s.recv(1024))
   s.close()
   PY
   ```

   Expected output:

   ```text
   b'boot\n'
   b'rx:A\n'
   ```

   Any tool that speaks raw TCP works against the same port. Copy the
   `HOST_ARDUINO_PORT=` value and connect with whatever is handy
   (substitute `34567` below with the actual port):

   ```bash
   # nc — preinstalled on Linux / macOS; on Windows via nmap or MSYS2
   nc 127.0.0.1 34567

   # socat — Linux / macOS (apt install socat / brew install socat)
   socat - TCP:127.0.0.1:34567

   # telnet — Windows optional feature; macOS via brew; Linux inetutils
   telnet 127.0.0.1 34567

   # PuTTY (Windows) — works headless from cmd / PowerShell
   putty.exe -raw 127.0.0.1 34567
   ```

   TeraTerm (Windows GUI): File → New Connection → TCP/IP, Service: Other,
   TCP port#: the printed value.

## TCP Serial Runtime

When the executable is started without internal runtime arguments, it acts as a launcher:

1. Starts a detached child process.
2. Waits for the child to create the connection-info file.
3. Prints the TCP port and info-file path to stdout.
4. Exits, allowing `arduino-cli upload` to return.

The child process opens a localhost TCP server, publishes the connection info,
and waits for the first TCP client before entering the Arduino sketch's
`setup()`. This keeps early `Serial.print()` / `Serial.println()` output from
being produced before pytest or another controller has finished attaching to
the TCP-backed `Serial` stream. If no client connects before
`HOST_ARDUINO_CONNECT_TIMEOUT_MS`, the child exits.

`Serial.print()`, `Serial.println()`, and `Serial.write()` append to a bounded
output buffer and are sent to the connected TCP client.

The connection-info file is written next to the executable:

```text
<executable>.host-arduino.json
```

Example:

```json
{
  "pid": 12345,
  "port": 34567
}
```

The runtime is designed for one test client per executable process. Multiple simultaneous TCP clients are not a requirement.

## File Paths

`SD.h`, `SPIFFS.h`, `LittleFS.h`, and `FFat.h` are mapped to host-side folders. Each filesystem root is placed next to the executable as `SD/`, `SPIFFS/`, `LittleFS/`, or `FFat/`. For example, `SD.open("/data.txt", FILE_WRITE)` opens `SD/data.txt` next to the executable.

These folders live under the build output location, so tests that need read fixtures should copy them into `SD/` or the relevant filesystem folder at the start of the test.

Direct `fopen()` calls from a sketch do not use this mapping. They follow the normal host C runtime behavior and resolve relative paths from the process current working directory. When launched from pytest, this is usually the directory containing `test.py`, so test folders such as `input/` and `output/` can be used directly. If you want direct `fopen()` paths to resolve next to the executable, change the current working directory from the test before or while launching the process.

## Runtime Environment Variables

- `HOST_ARDUINO_CONNECT_TIMEOUT_MS`: child process timeout while waiting for the first TCP client. Default: `10000`.
- `HOST_ARDUINO_START_DELAY_MS`: short settle delay after the first TCP client connects and before `setup()` starts. Default: `250`.
- `HOST_ARDUINO_PARENT_WAIT_MS`: launcher timeout while waiting for the child to publish connection info. Default: `5000`.
- `HOST_ARDUINO_SERIAL_BUFFER_SIZE`: maximum buffered Serial output bytes. Default: `65536`.
- `HOST_ARDUINO_LOG`: runtime log output. Default: `<executable>.host-arduino.log`. Set to `0`, `false`, or `off` to disable logging, or set a file path to override the destination.
- `HOST_ARDUINO_LOG_LEVEL`: runtime log level. Default: `info`. Use `debug` to include Serial byte counts.

After a client connects and the sketch starts, disconnecting the TCP socket
stops the sketch process.

The runtime log records launcher startup, child process startup, TCP listen/connect/disconnect events, Serial byte counts in debug mode, and the final exit reason. Serial payload bytes are not written to the log.

## Manual Build Without Arduino CLI

For quick local development of the core itself:

```bash
g++ -std=gnu++11 -Wall -I cores/host \
  -x c++ \
  /tmp/HostSmoke/HostSmoke.ino \
  cores/host/HostRuntime.cpp \
  cores/host/main.cpp \
  -pthread \
  -o /tmp/HostSmoke.out

/tmp/HostSmoke.out
```

Windows builds need Winsock linked in, so use `-lws2_32` instead of `-pthread`.

## Packaging and Release

The release workflow is handled by GitHub Actions.

Manual local packaging:

```bash
python3 scripts/build_package.py --version 0.1.0 --repo tanakamasayuki/host-arduino-core
```

This creates:

- `package/host-arduino-core/`
- `host-arduino-core-0.1.0.zip`
- updated `package_index.json`
- `package/package_index.json` for GitHub Pages publication

Release flow:

1. Update `CHANGELOG.md`: add entries under `## Unreleased`.
2. To inspect the release diff locally, run `python3 scripts/bump_version.py 0.1.0` to update `platform.txt` and the example `sketch.yaml` versions. The release workflow runs the same step.
3. Commit changes to `main`.
4. Push a tag such as `v0.1.0`, or run `Build and Release Host Arduino Core` manually from GitHub Actions.
5. The workflow updates `platform.txt` and the example `sketch.yaml` versions, moves the `## Unreleased` entries into `## <version>`, builds the ZIP, updates `package_index.json`, publishes `package/` to `gh-pages`, and attaches the ZIP plus index to the GitHub Release.
6. The GitHub Release body is populated from the matching `CHANGELOG.md` section, for example `## 0.1.0`.

## Limitations

- No hardware flashing is performed.
- `arduino-cli upload` means "start the host executable".
- `arduino-cli monitor` is not supported.
- No real or virtual serial port is provided.
- The TCP runtime is intended for localhost test automation.
- GPIO and analog APIs are minimal stubs, not hardware-accurate emulation.
- This core currently targets pure logic tests, not SDL2 or graphical host simulation.

Issues and feature requests are welcome in the tracker.
