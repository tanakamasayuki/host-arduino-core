"""Verifies that WiFiUDP emits [HostCore] hints when used without begin()."""

import re


def _expect_hint(dut, fragment, timeout=30):
    dut.expect(re.compile(re.escape(fragment).encode()), timeout=timeout)


def test_udp_no_begin(dut):
    dut.expect("TEST start no_begin", timeout=30)
    _expect_hint(dut, "[HostCore] WiFiUDP::beginPacket() called before begin()")
    _expect_hint(dut, "[HostCore] WiFiUDP::endPacket() called before begin()")
    _expect_hint(dut, "[HostCore] WiFiUDP::parsePacket() called before begin()")
    dut.expect("TEST done", timeout=30)
