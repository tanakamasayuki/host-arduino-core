"""interop/https_get — HTTPS round-trip parity between host (OpenSSL)
and real ESP32 (mbedTLS), using httpbin.org as a shared peer.

Strategy: the sketch sends a unique value via an `X-Test-Tag` request
header. `httpbin.org/get` echoes incoming headers in its JSON
response, so finding the same value anywhere in the body confirms the
full request → handshake → response → body-decode pipeline worked on
either target.
"""

import re


TAG = b"host-arduino-core-interop"


def test_interop_https_get(dut):
    # Wi-Fi association (real on ESP32, instant stub on host).
    match = dut.expect(
        [
            re.compile(rb"WIFI_OK ((\d{1,3}\.){3}\d{1,3})"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=70,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connect failed: {match.group(1).decode()}")

    # HTTPS request — give httpbin.org generous headroom for both DNS
    # resolution and TLS handshake on a fresh boot.
    dut.expect(re.compile(rb"CODE=200"), timeout=60)

    body_len_match = dut.expect(re.compile(rb"BODY_LEN=(\d+)"), timeout=60)
    body_len = int(body_len_match.group(1))
    assert body_len > 0, "empty body from httpbin.org"

    # The unique tag we sent in the request header must appear in
    # httpbin.org's echoed JSON. This is the actual parity check.
    dut.expect(re.compile(re.escape(TAG)), timeout=60)

    dut.expect(re.compile(rb"BODY_END"), timeout=60)
    dut.expect(re.compile(rb"DONE"), timeout=60)
