"""interop/wifi_connect — WiFi.begin / status / localIP shape parity
between the host stub and real ESP32 silicon.

Requires `TEST_WIFI_SSID` and `TEST_WIFI_PASSWORD` in the env
(typically loaded via `uv run --env-file .env`). The host runtime
ignores the values (the WiFi facade is a stub) but the build_config
mapping still injects them as `-DWIFI_SSID="..."` so the sketch text
is identical on both targets.
"""

import re


def test_wifi_connect(dut):
    match = dut.expect(
        [
            re.compile(rb"WIFI_OK ((\d{1,3}\.){3}\d{1,3})"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=70,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connect failed: {match.group(1).decode()}")
