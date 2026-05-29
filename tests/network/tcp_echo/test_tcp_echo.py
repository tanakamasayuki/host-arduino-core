"""TCP echo via WiFiServer + accepted WiFiClient."""

import re
import socket


def test_tcp_echo(dut):
    match = dut.expect(re.compile(rb"TCP_PORT=(\d+)"), timeout=30)
    sketch_port = int(match.group(1))
    assert sketch_port > 0

    sock = socket.create_connection(("127.0.0.1", sketch_port), timeout=5)
    sock.settimeout(5.0)
    try:
        dut.expect(re.compile(rb"ACCEPTED"), timeout=5)

        for payload in (b"hello\n", b"second message\n", bytes(range(64))):
            sock.sendall(payload)
            dut.expect(
                re.compile(rb"ECHO " + str(len(payload)).encode() + rb"\b"),
                timeout=5,
            )

            received = b""
            while len(received) < len(payload):
                chunk = sock.recv(len(payload) - len(received))
                if not chunk:
                    break
                received += chunk
            assert received == payload
    finally:
        sock.close()

    dut.expect(re.compile(rb"CLOSED"), timeout=5)
