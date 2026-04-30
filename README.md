# host-arduino-core

Host Arduino Core is a minimal Arduino Core and Boards Manager package for testing Arduino sketch logic on the host PC.

- FQBN: `lang-ship:host:host`
- Core directory: `cores/host`
- Build toolchain: host-provided `gcc` / `g++` compatible tools
- `arduino-cli compile`: builds a host executable
- `arduino-cli upload`: starts the executable in the background
- `arduino-cli monitor`: not supported

The runtime exposes `Serial` over a localhost TCP connection. On startup, the launcher prints the selected TCP port to stdout and also writes a JSON file next to the executable:

```text
<executable>.host-arduino.json
```

The JSON file contains `pid` and `port`. It is intended as a fallback for test frameworks when stdout handling differs across operating systems.

This package does not install a compiler, linker, or other build tools through Boards Manager. Install a suitable host `gcc` / `g++` compatible toolchain before using it.
