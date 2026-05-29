"""interop/http_redirect — multi-hop HTTPS redirect parity between
host (OpenSSL backend) and real ESP32 silicon (mbedTLS backend).

httpbin.org/redirect/3 issues three 302 responses before landing on
/get. With HTTPC_FORCE_FOLLOW_REDIRECTS the final status must be 200
and the body must be httpbin's standard /get JSON, which always
contains the literal `httpbin.org/get` in its `url` field — that
substring is the parity marker.

Note: user-added request headers (addHeader values) are intentionally
dropped across redirects on both runtimes to match ESP32 Arduino
HTTPClient v3.x behavior, so the test does NOT assert their presence
in the final body.
"""

import re


def test_interop_http_redirect(dut):
    match = dut.expect(
        [
            re.compile(rb"WIFI_OK ((\d{1,3}\.){3}\d{1,3})"),
            re.compile(rb"WIFI_ERROR ([^\r\n]+)"),
        ],
        timeout=70,
    )
    if match.re.pattern.startswith(b"WIFI_ERROR"):
        raise AssertionError(f"Wi-Fi connect failed: {match.group(1).decode()}")

    # Three 302 hops + a final 200 across HTTPS — give DNS/TLS room.
    dut.expect(re.compile(rb"CODE=200"), timeout=60)

    body_len_match = dut.expect(re.compile(rb"BODY_LEN=(\d+)"), timeout=30)
    assert int(body_len_match.group(1)) > 0, "empty body after redirect chain"

    # Stable marker that confirms we landed on httpbin's /get endpoint
    # after the redirect chain — httpbin echoes its own URL there.
    dut.expect(re.compile(rb"httpbin\.org/get"), timeout=30)

    dut.expect(re.compile(rb"BODY_END"), timeout=30)
    dut.expect(re.compile(rb"DONE"), timeout=30)
