# host-arduino-core

English | 日本語: [README.ja.md](README.ja.md)

https://tanakamasayuki.github.io/lang-ship-arduino-core/package_lang-ship_index.json

Minimal Arduino Core + Boards Manager package for building Arduino sketches as host executables and driving them from Arduino CLI based host-side tests such as `pytest-embedded-arduino-cli`.

This is not a practical Arduino-compatible board. It is a test target for pure sketch/library logic that does not require real hardware, flashing, or a serial port.

## Highlights

- Architecture `host`, FQBN `lang-ship:host:host`.
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
| `Preferences` (NVS) | 🔲 | ESP32 | could be backed by a file next to the executable |
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
| `Client` / `Server` (abstract) | 🔲 | Arduino | prerequisite for any TCP impl |
| `WiFiClient` (TCP) | 🔲 | Arduino / ESP32 | doable on POSIX sockets, ~250 LOC |
| `WiFiServer` (TCP) | 🔲 | Arduino / ESP32 | doable on POSIX sockets, ~120 LOC |
| `WiFiClientSecure` (TLS) | ⛔ | ESP32 | would need mbedTLS / OpenSSL — out of scope here |
| `HTTPClient` | ⛔ | ESP32 | belongs in a separate library on top of `Client` |
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
| FreeRTOS (`xTaskCreate`, `vTaskDelay`, queues, semaphores, mutexes, notifications) | 🔲 | ESP-IDF | could be backed by `std::thread` / `std::mutex` / `std::condition_variable`, ignoring priority, core affinity, and stack size. Useful for the common "periodic worker task" pattern. Non-goals: priority-based scheduling, core pinning, `vTaskSuspend` immediacy, stack-overflow detection, `portENTER_CRITICAL` as real interrupt masking — sketches that rely on those should not be tested on host |
| `esp_log` / `log_e` / `log_i` / `log_w` / `log_d` macros | 🔲 | ESP-IDF | could route to `Serial` |
| `esp_timer` | 🔲 | ESP-IDF | thin wrapper over `millis` / `micros` |
| `esp_random` / `esp_fill_random` | 🔲 | ESP-IDF | trivial |
| `Preferences` (NVS) | 🔲 | ESP32 | see Filesystem row |
| Raw `nvs_flash_*` API | ⛔ | ESP-IDF | use `Preferences` instead |
| `Update` / OTA | ⛔ | ESP32 | meaningless on host |
| BLE / Classic Bluetooth | ⛔ | ESP32 | no plan |
| ESP-NOW | 🔲 | ESP32 | could be faked over UDP broadcast, but low priority |
| Camera (`esp_camera`) | ⛔ | ESP32 | out of scope |

### Graphics / Display

| API | Status | Notes |
|-----|--------|-------|
| M5GFX framebuffer mode + screen capture | 🔲 | draw to a framebuffer and persist as an image file (see `TODO.md`) |
| LovyanGFX / TFT_eSPI | 🔲 | same approach as M5GFX |

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
  network/  udp_recv, udp_echo, udp_broadcast, udp_no_begin, wifi
```

Each leaf has a `.ino` + `sketch.yaml` + `test_*.py`. New tests prove a
square in the matrix goes green.

## Board

The initial package provides a single board:

```text
lang-ship:host:host
```

- Package: `lang-ship`
- Architecture: `host`
- Board ID: `host`
- Board name: `Host`
- Core directory: `cores/host`

No board menus are defined in the initial version. SDL2 and graphical host targets are intentionally out of scope for now.

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

## TCP Serial Runtime

When the executable is started without internal runtime arguments, it acts as a launcher:

1. Starts a detached child process.
2. Waits for the child to create the connection-info file.
3. Prints the TCP port and info-file path to stdout.
4. Exits, allowing `arduino-cli upload` to return.

The child process runs the Arduino sketch and opens a localhost TCP server. `Serial.print()`, `Serial.println()`, and `Serial.write()` append to a bounded output buffer. Data printed before the test connects is sent to the first TCP client after connection.

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
- `HOST_ARDUINO_PARENT_WAIT_MS`: launcher timeout while waiting for the child to publish connection info. Default: `5000`.
- `HOST_ARDUINO_SERIAL_BUFFER_SIZE`: maximum buffered Serial output bytes. Default: `65536`.
- `HOST_ARDUINO_LOG`: runtime log output. Default: `<executable>.host-arduino.log`. Set to `0`, `false`, or `off` to disable logging, or set a file path to override the destination.
- `HOST_ARDUINO_LOG_LEVEL`: runtime log level. Default: `info`. Use `debug` to include Serial byte counts.

If no TCP client connects before the connect timeout, the child process exits. After a client connects, disconnecting the TCP socket also stops the sketch process.

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
