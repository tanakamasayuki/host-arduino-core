"""interop/http_redirect — multi-hop HTTPS redirect parity between
host (OpenSSL backend) and real ESP32 silicon (mbedTLS backend).

httpbin.org/redirect/3 issues three 302 responses before landing on
/get. With HTTPC_FORCE_FOLLOW_REDIRECTS the final status must be 200,
and the X-Test-Tag header set on the original request must survive
across hops and surface in the echoed JSON body.
"""

import re


TAG = b"host-arduino-core-interop"


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

    body_len_match = dut.expect(re.compile(rb"BODY_LEN=(\d+)"), timeout=10)
    assert int(body_len_match.group(1)) > 0, "empty body after redirect chain"

    # The header sent on the first request must be present in the
    # final response's echoed JSON, confirming that addHeader() values
    # persist across redirects on both runtimes.
    dut.expect(re.compile(re.escape(TAG)), timeout=15)

    dut.expect(re.compile(rb"BODY_END"), timeout=10)
    dut.expect(re.compile(rb"DONE"), timeout=5)
