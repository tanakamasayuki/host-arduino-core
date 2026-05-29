"""Tests for the WiFi stub status / localIP / SSID transitions."""

import re


def test_wifi(dut):
    dut.expect("TEST start wifi", timeout=30)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=15)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0, "no assertions ran"
