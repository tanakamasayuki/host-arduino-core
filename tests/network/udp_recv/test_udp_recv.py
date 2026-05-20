"""One-way UDP: pytest sends, sketch reports payload over Serial."""

import re
import socket


def test_udp_recv(dut):
    match = dut.expect(re.compile(rb"UDP_PORT=(\d+)"), timeout=10)
    port = int(match.group(1))
    assert port > 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    try:
        sock.sendto(b"hello-udp", ("127.0.0.1", port))
        m = dut.expect(
            re.compile(rb"RX from 127\.0\.0\.1:(\d+) (\d+) (\S+)"),
            timeout=5,
        )
        sender_port = int(m.group(1))
        length = int(m.group(2))
        payload = m.group(3).decode()
        assert sender_port == sock.getsockname()[1]
        assert length == len(b"hello-udp")
        assert payload == "hello-udp"
    finally:
        sock.close()
