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

### Two-tier rule (host first → interop on top)

When adding tests for a new feature or bug fix, **cover it under a
host-only category (`runtime/` / `storage/` / `network/`) first, then
add an `interop/` test only if real-silicon verification is needed**.

- **Interop tests do not replace host tests.** Don't skip the
  host-side test just because you added an interop one.
- **Content overlap is fine.** Host-side tests exercise edge cases
  and internal behavior against a local fixture; interop tests
  exercise the same feature against real hardware and an external
  service, asserting only surface parity. Hitting the same code path
  through two paths is the point.
- **If a check is reproducible on the host, the host-only test is
  enough.** Only add `interop/` for things that can't be meaningfully
  exercised without real silicon — host-vs-ESP32 divergence-prone
  behavior like TLS handshake, redirect following, chunked decoding.

Rationale: without a host counterpart, a code path is unprotected
against every-commit regressions. The only failure mode `interop/`
exclusively catches is "works on host, breaks on silicon" — for
everything else, the host-only test is the faster and more reliable
signal.

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

## File I/O convention

When a sketch needs to read fixture data or write generated artifacts:

- **Working directory at runtime is the sketch directory** (the one
  containing the `.ino`). pytest-embedded launches the binary from
  there, so plain `fopen("foo", ...)` resolves against the sketch dir.
- **Inputs go under `<sketch_dir>/input/`** — check fixture files into
  git there. The sketch reads them via `fopen("input/foo", "rb")`.
- **Outputs go under `<sketch_dir>/output/`**. The sketch is expected
  to `mkdir("output", 0755)` (ignore EEXIST) then write to
  `output/<name>` via `fopen`. `output/` should be in the project's
  `.gitignore` so artifacts never get committed.
- **Wipe `output/` before each test run.** Stale files from a previous
  run could otherwise make a failing sketch look like it succeeded.
  In this project the wipe is implemented in `tests/conftest.py`
  (`pytest_runtest_setup` hook removes `<sketch_dir>/output/` before
  each test); a downstream project that doesn't already have a
  conftest can copy that hook as a starting point, or arrange the
  same cleanup with whatever fixture / setup mechanism it uses.
- The test then asserts on the file under `<sketch_dir>/output/<name>`.

This keeps source files (`.ino`, `sketch.yaml`, `test_*.py`) and
generated files visibly separated, and makes "clean run" the default
for every test invocation.

`tests/graphics/lgfx_smoke` is the reference implementation — its
`output/lgfx_smoke_capture.png` is produced by `gfx.createPng()` and
asserted by the test.

## Sharing code between sketches via `tests/common_libs/`

Tests that need to share C++ code (drawing helpers, mock state, etc.) put
it under `tests/common_libs/<libname>/` as an Arduino-library-shaped
directory (`library.properties` + `src/`). Sketches reference it from
their `sketch.yaml`:

```yaml
profiles:
  host:
    libraries:
      - dir: ../../common_libs/<libname>
```

`tests/common_libs/gfx_demo/` is the reference. It declares `drawHome`
and friends, used identically by the three graphics smokes
(`lovyangfx_smoke`, `m5gfx_smoke`, `m5unified_smoke`). The library
implements the routine once and renders it at many sizes / rotations /
color depths via `LGFX_Sprite` inside each sketch, so regressions in
the routine surface against all three library entry points
simultaneously.

### Library that targets multiple upstream graphics libs

`gfx_demo.cpp` picks one of `M5Unified.h` / `M5GFX.h` / `LovyanGFX.hpp`
via `__has_include`. This 3-way switch is **a test-only pattern** —
production code should pin a single graphics library and include it
directly. With `sketch.yaml` the `__has_include` result is
deterministic (only declared libraries appear on the include path);
the Arduino IDE, with multiple libraries installed globally, can give
unexpected results.

`gfx_demo.h` is intentionally kept "neutral": it has no `__has_include`
of its own and declares only `void drawHome(LovyanGFX &)`. Callers must
include their graphics library before `#include <gfx_demo.h>` so the
`LovyanGFX` type is in scope. This keeps the 3-way switch confined to
the single `.cpp`.

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
