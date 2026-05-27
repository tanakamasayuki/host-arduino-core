# tests

English | 日本語: [README.ja.md](README.ja.md)

Local tests for `host-arduino-core`, built on
[`pytest-embedded-arduino-cli`](https://pypi.org/project/pytest-embedded-arduino-cli/).
Tests are grouped by area:

```
tests/
  runtime/   # host-only — Arduino runtime basics (smoke, timing, print_api)
  storage/   # host-only — LittleFS/SPIFFS/FFat/SD facades (fs)
  network/   # host-only — IPAddress / UDP / TCP / TLS / HTTPClient
  interop/   # host + real ESP32 — implementation-parity checks (smoke, wifi_connect, https_get, ...)
```

Each leaf directory is one test target (sketch + `sketch.yaml` +
`test_*.py`). Add new categories by creating new top-level directories.

## Test policy

### Host-only tests (`runtime/` / `storage/` / `network/`)

- **What they verify**: the host runtime itself — socket backends,
  `Stream` behavior, `HostDiag` hints, `HTTPClient` parsing and state
  machine, anything that lives entirely on the host side.
- **When to run**: every time you edit the core. Pre-review,
  pre-commit, pre-push checklist.
- **Zero hardware dependency**: runs locally, no external network or
  USB device required. Full suite finishes in a few minutes.
- **Coverage-first**: edge cases, hint messages, error paths — all
  fair game. If a test is easy to write on the host, put it here.

### Interop tests (`interop/`)

- **What they verify**: behavioral parity between the host runtime
  and real ESP32 silicon. The areas that matter are the ones where
  the two implementations are likely to diverge — TLS handshake,
  `HTTPClient` redirect following, chunked decoding, Wi-Fi flow, etc.
- **When to run**: **manually**, only when you touch the relevant
  code path. Running them on every change is not worth the cost
  (real ESP32 + Wi-Fi + external service reachability).
- **Manual-trigger checklist**:
  - You changed `WiFiClient` / `WiFiClientSecure` / `HTTPClient` /
    `WiFiUDP`.
  - You added a new `tls` menu option or a backend branch.
  - You touched the redirect / chunked / header logic in `HTTPClient`.
  - You're about to write a sketch that talks to an external API.
- **No `#ifdef ARDUINO_ARCH_HOST`**: the sketch must build from
  **identical source** under both profiles. Source identity is what
  makes the test a real parity check.
- **Only assert observable behavior**: internal implementations
  diverge (host uses OpenSSL, ESP32 uses mbedTLS); cipher names,
  internal buffer sizes, memory layouts are out of scope. Only the
  Arduino API surface (status codes, body bytes, Serial output order)
  counts as evidence.

## Criteria for adding a new interop test

Before adding a test under `interop/`, check it against this list:

1. **Is the area likely to diverge between host and silicon?** TLS
   (OpenSSL vs mbedTLS), HTTP redirect / chunked, Wi-Fi flow,
   `Client::connected()` semantics — anything near an ABI or protocol
   boundary is a candidate. Pure C++ runtime behavior (e.g.,
   `String::concat`) is spec-bound and usually not.
2. **Is "works on host, breaks on silicon" plausible?** The whole
   point of interop is to catch surprises like the redirect-header
   persistence divergence we caught (host preserved them, ESP32
   dropped them). If both sides obviously do the same thing, the test
   has nothing to defend against.
3. **Can the test be written identically for both profiles?** The
   sketch must run the same code under `dut.write` / `dut.expect`
   against either an external service (httpbin.org) or a Serial
   protocol. If the silicon side needs special setup, prefer a manual
   smoke check instead of an interop test.
4. **Does the test tie back to a specific code change?** Write the
   test against a concrete behavior a change might break, not "just
   in case".
5. **Is the external dependency acceptable?** httpbin.org / badssl.com
   reachability can be flaky. If the same scenario can be exercised
   against a local Python server, put it in `network/` instead.
   Reserve interop for cases where only real silicon makes sense.

## Setup

Install test deps:

```bash
cd tests
uv sync          # or: pip install -e .
```

The session-scoped fixture in `conftest.py` automatically symlinks this
repository into `<sketchbook>/hardware/lang-ship/host` at the start of the
session and removes it at the end, so `lang-ship:host:host` resolves to the
local working tree while the tests run.

If `<sketchbook>/hardware/lang-ship/host` already exists and points
somewhere else (e.g. a manual install), the fixture fails fast — remove or
repoint that entry before running.

### `.env` (interop only)

Wi-Fi-using interop tests pull SSID / password from `.env` via
build-time `-D` injection (start from
[.env.example](.env.example)):

```
TEST_SERIAL_PORT_ESP32=/dev/ttyUSB0
TEST_WIFI_SSID=YourSSID
TEST_WIFI_PASSWORD=YourPassword
```

Each interop directory that needs it has a `build_config.toml` that
declares the `TEST_WIFI_SSID` → `-DWIFI_SSID="..."` mapping.

## Run

### Host-only tests (everyday)

```bash
cd tests
uv run pytest -v
```

Everything completes on the host. No Wi-Fi, no external service.
`interop/` is excluded from default discovery via
`addopts = "--ignore=interop"` in `pyproject.toml` (blacklist).
Explicit invocations like `pytest interop/` still work — `--ignore`
only suppresses recursive discovery, not paths passed on the command
line. New top-level categories are auto-included by default; only
manual / hardware-dependent ones need an entry here.

### Interop tests (host profile)

```bash
cd tests
uv run --env-file .env pytest -v interop/
```

Without `--env-file .env` the WiFi credentials are missing and the
tests will fail. Running against the host profile at least proves that
the same sketch source builds in both profiles and that the Arduino
API path works under the host runtime.

### Interop tests (real ESP32)

```bash
cd tests
uv run --env-file .env pytest --profile esp32 interop/
```

`--profile esp32` selects the `esp32` profile from `sketch.yaml`,
builds against `esp32:esp32:esp32` (3.3.8), flashes over USB, and
collects results over the serial port. Intended to be run **manually
when you've touched the relevant code path**, not on every commit.

`runtime/`, `storage/`, and `network/` do not declare an `esp32`
profile, so `--profile esp32` simply skips them (pytest-embedded
treats the missing profile as a profile mismatch).

## How the local-version trick works

`sketch.yaml` pins the platform **without a version number and without a
`platform_index_url`**:

```yaml
platforms:
  - platform: lang-ship:host
```

With no version pin, arduino-cli reuses whatever `lang-ship:host` is already
installed — including the sketchbook `hardware/lang-ship/host` symlink — so
the build runs against the local source tree. Edit the core, re-run pytest,
done. No release needed.

To verify a *released* version instead, add the version and index URL back,
e.g.:

```yaml
- platform: lang-ship:host (1.0.5)
  platform_index_url: https://tanakamasayuki.github.io/host-arduino-core/package_index.json
```

The `esp32` profile is the opposite — it pins the version + index URL
explicitly because we want the Boards-Manager-installed package, not a
local working tree:

```yaml
esp32:
  fqbn: esp32:esp32:esp32
  platforms:
    - platform: esp32:esp32 (3.3.8)
      platform_index_url: https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

## Add a new test

### Host-only test

```
tests/<category>/<name>/
  <name>.ino
  sketch.yaml      # copy from any existing host-only test
  test_<name>.py   # uses the `dut` fixture
```

The `.ino` filename must match the directory name (arduino-cli convention).

### Interop test

```
tests/interop/<name>/
  <name>.ino                # Arduino sketch that builds under both profiles
  sketch.yaml               # host + esp32 profiles
  build_config.toml         # env → -D injection (e.g., WIFI_SSID), if needed
  test_interop_<name>.py
```

- Don't reuse `test_<name>.py` basenames that already exist in the
  tree — pytest fails on module-name collisions. Prefix interop tests
  with `test_interop_` to keep them distinct.
- Always start the sketch with `Serial.begin(115200); delay(5000);`
  — required by the ESP32 USB-Serial bridge settle window and
  harmless on host (`delay` is a thin `sleep_for` wrapper).
- Do not use `#ifdef ARDUINO_ARCH_HOST` or similar to fork the source
  per profile (policy).
- Keep assertions at the Arduino API surface — status codes, body
  bytes, observable side effects. Do not rely on cipher names,
  internal buffer sizes, or memory layout.
