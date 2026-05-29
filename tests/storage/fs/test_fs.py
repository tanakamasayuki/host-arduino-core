"""Tests for the host FS facades (LittleFS / SPIFFS / FFat / SD)."""

import re


def test_fs(dut):
    dut.expect("TEST start fs", timeout=10)
    match = dut.expect(re.compile(rb"TEST done (\d+)/(\d+)"), timeout=10)
    passed, total = int(match.group(1)), int(match.group(2))
    assert passed == total, f"{total - passed} of {total} assertions failed"
    assert total > 0, "no assertions ran"
