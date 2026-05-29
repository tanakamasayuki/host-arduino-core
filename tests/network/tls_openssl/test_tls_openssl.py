"""Verify that the `tls=openssl` board menu option links libssl and
that OpenSSL is reachable from a sketch at both compile time and
runtime.

This test only runs on environments that have OpenSSL development
headers / libraries installed. On Linux: `apt install libssl-dev`.
"""

import re


def test_tls_openssl(dut):
    header = dut.expect(
        re.compile(rb"OPENSSL_HEADER=(OpenSSL[^\r\n]+)"),
        timeout=10,
    ).group(1).decode()
    runtime = dut.expect(
        re.compile(rb"OPENSSL_RUNTIME=(OpenSSL[^\r\n]+)"),
        timeout=10,
    ).group(1).decode()

    assert header.startswith("OpenSSL "), f"unexpected header version: {header!r}"
    assert runtime.startswith("OpenSSL "), f"unexpected runtime version: {runtime!r}"

    dut.expect(re.compile(rb"CTX_NEW_OK"), timeout=10)
    dut.expect(re.compile(rb"TLS_PROBE_DONE"), timeout=10)
