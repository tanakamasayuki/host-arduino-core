# host-arduino-core

English | 日本語: [README.ja.md](README.ja.md)

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
