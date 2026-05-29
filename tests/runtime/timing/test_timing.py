"""Tests for millis / micros / delay / delayMicroseconds."""

import re


def test_timing(dut):
    dut.expect("TEST start timing", timeout=30)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=30)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0, "no assertions ran"
