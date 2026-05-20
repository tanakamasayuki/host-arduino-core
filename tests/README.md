# tests

English | 日本語: [README.ja.md](README.ja.md)

Local tests for `host-arduino-core`, built on
[`pytest-embedded-arduino-cli`](https://pypi.org/project/pytest-embedded-arduino-cli/).
Each subdirectory under `tests/` is one test target (sketch + `sketch.yaml`
+ `test_*.py`). `smoke/` is the minimal build-and-boot check; add more
alongside it as needed.

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

## Run

```bash
cd tests
uv run pytest -v
```

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

## Add a new test

```
tests/<name>/
  <name>.ino
  sketch.yaml      # copy from smoke/sketch.yaml
  test_<name>.py   # uses the `dut` fixture
```
