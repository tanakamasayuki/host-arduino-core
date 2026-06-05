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
| `setup()` / `loop()` / weak `main` | ✅ | Arduino | `cores/host/main.cpp` |
| `millis` / `micros` | ✅ | Arduino | `std::chrono::steady_clock` |
| `delay` / `delayMicroseconds` | ✅ | Arduino | `std::this_thread::sleep_for` |
| `yield` | ✅ | Arduino | no-op |
| `min` / `max` / `constrain` / `map` / `abs` | ✅ | Arduino | header-only |
| `random` / `randomSeed` | ✅ | Arduino | wraps `std::rand` |
| `bit*` / `lowByte` / `highByte` / `_BV` | ✅ | Arduino | macros |
| `String` (`WString.h`) | ✅ | Arduino | |

### Print / Stream / Serial

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `Print` (int / hex / bin / float / String / bool) | ✅ | Arduino | matches Arduino formatting |
| `Printable` | ✅ | Arduino | |
| `Stream` (`timedRead` / `readBytes` / `setTimeout` / `find` / `parseInt`) | ✅ | Arduino | |
| `HardwareSerial` / `Serial` | ✅ | Arduino | exposed over a localhost TCP socket |
| `Serial1` / `Serial2` | 🔲 | ESP32 | only needed for sketches that drive multiple UARTs |

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

### Hardware I/O (intentionally stubbed)

These are stubbed so that real-hardware sketches at least link. Tests that
care about pin state should mock at the sketch layer.

| API | Status | Source | Notes |
|-----|--------|--------|-------|
| `pinMode` / `digitalWrite` / `digitalRead` | 🟡 | Arduino | no-op stubs; `digitalRead` returns `0` |
| `analogRead` / `analogWrite` / `analogReadResolution` / `analogSetAttenuation` | 🟡 | Arduino / ESP32 | no-op stubs |
| `touchRead` / `touchAttachInterrupt` | 🟡 | ESP32 | returns `0` |
| `tone` / `noTone` / `pulseIn` / `pulseInLong` | 🟡 | Arduino | no-op stubs |
| `attachInterrupt` / `detachInterrupt` | 🟡 | Arduino | no-op stubs |
| `dacWrite` / `ledcWrite` / `ledcAttach` / `ledcSetup` | 🔲 | ESP32 | no-op stubs would be sufficient |
| `Wire` (I²C) | 🔲 | Arduino | "init succeeds, no device present" stub is the planned shape |
| `SPI` | 🔲 | Arduino | same shape as `Wire` |
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
  runtime/  smoke, timing, print_api
  storage/  fs
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

M5Unified `M5_LOGE()` / `M5_LOGW()` / `M5_LOGI()` / `M5_LOGD()` /
`M5_LOGV()` expand to `M5.Log`, not ESP32 `ESP_LOG*`. In PC / SDL2 builds,
M5Unified prints those logs to stdout, so manual checks keep the SDL2 window
and log console separate.

## Repository Layout

- `platform.txt` / `boards.txt`: Arduino platform metadata copied into the release ZIP.
- `cores/host/Arduino.h`: minimal Arduino-facing API surface.
- `cores/host/HostRuntime.{h,cpp}`: host runtime, TCP-backed `Serial`, process launcher, and connection-info file handling.
- `cores/host/main.cpp`: weak `main()` that calls `setup()` once and then `loop()` until the runtime requests shutdown.
- `scripts/build_package.py`: creates `package/host-arduino-core/`, produces the ZIP, computes SHA-256, and updates `package_index.json`.
- `scripts/prepare_release.py`: moves `CHANGELOG.md` unreleased entries into the release version section.
- `.github/workflows/release.yml`: builds/releases the package on tag push or manual dispatch, publishes `package/` to `gh-pages`, and attaches assets to GitHub Releases.
- `CHANGELOG.md`: release notes in English and Japanese. The release workflow uses the matching version section as the GitHub Release body.
- `docs/requirements.ja.md`: requirements document.
- `examples/`: ready-to-open sketches shipped in the release ZIP (e.g. `TLSProbe` for verifying the `tls=openssl` board menu option on each OS).
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
2. Commit changes to `main`.
3. Push a tag such as `v0.1.0`, or run `Build and Release Host Arduino Core` manually from GitHub Actions.
4. The workflow moves the `## Unreleased` entries into `## <version>`, builds the ZIP, updates `package_index.json`, publishes `package/` to `gh-pages`, and attaches the ZIP plus index to the GitHub Release.
5. The GitHub Release body is populated from the matching `CHANGELOG.md` section, for example `## 0.1.0`.

## Limitations

- No hardware flashing is performed.
- `arduino-cli upload` means "start the host executable".
- `arduino-cli monitor` is not supported.
- No real or virtual serial port is provided.
- The TCP runtime is intended for localhost test automation.
- GPIO and analog APIs are minimal stubs, not hardware-accurate emulation.
- This core currently targets pure logic tests, not SDL2 or graphical host simulation.

Issues and feature requests are welcome in the tracker.
