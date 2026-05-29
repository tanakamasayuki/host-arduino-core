"""Verifies WiFiUDP enables SO_BROADCAST so sending to a broadcast address works.

Uses 127.255.255.255 (loopback subnet broadcast) instead of 255.255.255.255
so the test passes on macOS CI where there is no real NIC with a broadcast route.
Without SO_BROADCAST, sendto() to a broadcast address returns EACCES on Linux
and endPacket() reports 0.
"""

import re


def test_udp_broadcast(dut):
    match = dut.expect(re.compile(rb"UDP_PORT=(\d+)"), timeout=60)
    port = int(match.group(1))
    assert port > 0

    m = dut.expect(re.compile(rb"BCAST_SENT=(\d+)"), timeout=60)
    assert int(m.group(1)) == 1, "broadcast sendto failed — SO_BROADCAST likely not set"

    m = dut.expect(re.compile(rb"SELF_SENT=(\d+)"), timeout=60)
    assert int(m.group(1)) == 1, "unicast sendto failed"

    dut.expect("RX self", timeout=60)
