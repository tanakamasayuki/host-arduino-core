"""interop/http_chunked — HTTPS + Transfer-Encoding: chunked decoding
parity between host (OpenSSL) and real ESP32 (mbedTLS).

httpbin.org/stream/3 returns three JSON records over a chunked-encoded
response with no Content-Length. The sketch counts how many times the
unique X-Test-Tag value appears in the decoded body (httpbin echoes
incoming headers per record), which should equal 3 on both targets if
chunked decoding lands all records intact.
"""

import re


def test_interop_http_chunked(dut):
    match = dut.expect(
        [
            re.compile(rb"WIFI_OK ((\d{1,3}\.){3}\d{1,3})"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=70,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connect failed: {match.group(1).decode()}")

    dut.expect(re.compile(rb"CODE=200"), timeout=30)

    # Chunked responses have no Content-Length, so HTTPClient::getSize()
    # is -1 — that itself is the parity check for this code path.
    dut.expect(re.compile(rb"LEN=-1"), timeout=5)

    body_len_match = dut.expect(re.compile(rb"BODY_LEN=(\d+)"), timeout=10)
    assert int(body_len_match.group(1)) > 0, "empty body from /stream/3"

    # The unique tag must appear once per echoed record — three times
    # for /stream/3. This catches both partial decode and over-decode.
    dut.expect(re.compile(rb"TAG_COUNT=3"), timeout=5)

    dut.expect(re.compile(rb"BODY_END"), timeout=10)
    dut.expect(re.compile(rb"DONE"), timeout=5)
