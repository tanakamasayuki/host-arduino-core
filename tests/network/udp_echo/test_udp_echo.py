"""Bidirectional UDP echo round-trip."""

import re
import socket


def test_udp_echo(dut):
    match = dut.expect(re.compile(rb"UDP_PORT=(\d+)"), timeout=60)
    sketch_port = int(match.group(1))
    assert sketch_port > 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    sock.settimeout(5.0)
    try:
        for payload in (b"hello", b"second message", bytes(range(64))):
            sock.sendto(payload, ("127.0.0.1", sketch_port))
            dut.expect(
                re.compile(rb"ECHO " + str(len(payload)).encode() + rb"\b"),
                timeout=60,
            )
            data, addr = sock.recvfrom(2048)
            assert data == payload
            assert addr == ("127.0.0.1", sketch_port)
    finally:
        sock.close()
