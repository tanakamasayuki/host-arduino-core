"""Verifies WiFiUDP enables SO_BROADCAST so sending to 255.255.255.255 works.

Without SO_BROADCAST, sendto() to a broadcast address returns EACCES on
Linux and endPacket() reports 0. We assert it reports 1, and also confirm
that a normal unicast send still works (sanity check).
"""

import re


def test_udp_broadcast(dut):
    match = dut.expect(re.compile(rb"UDP_PORT=(\d+)"), timeout=10)
    port = int(match.group(1))
    assert port > 0

    m = dut.expect(re.compile(rb"BCAST_SENT=(\d+)"), timeout=5)
    assert int(m.group(1)) == 1, "broadcast sendto failed — SO_BROADCAST likely not set"

    m = dut.expect(re.compile(rb"SELF_SENT=(\d+)"), timeout=5)
    assert int(m.group(1)) == 1, "unicast sendto failed"

    dut.expect("RX self", timeout=5)
