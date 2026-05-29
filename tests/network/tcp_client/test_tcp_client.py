"""WiFiClient.connect() — sketch is the TCP client, Python is the server."""

import re
import socket
import threading


def test_tcp_client(dut):
    dut.expect(re.compile(rb"READY"), timeout=60)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(10.0)
    server_port = listener.getsockname()[1]

    accepted: dict = {}

    def accept_one():
        conn, _ = listener.accept()
        conn.settimeout(5.0)
        accepted["sock"] = conn

    t = threading.Thread(target=accept_one, daemon=True)
    t.start()

    try:
        dut.write(f"CONNECT {server_port}\n".encode())
        dut.expect(re.compile(rb"CONNECTING " + str(server_port).encode()), timeout=60)

        t.join(timeout=60)
        assert "sock" in accepted, "sketch did not connect"
        conn: socket.socket = accepted["sock"]

        dut.expect(re.compile(rb"CONNECTED"), timeout=60)
        dut.expect(re.compile(rb"WROTE 4"), timeout=60)

        # Read what the sketch sent us.
        received = b""
        while len(received) < 4:
            chunk = conn.recv(4 - len(received))
            if not chunk:
                break
            received += chunk
        assert received == b"PING"

        # Send a response — sketch should echo it back on Serial.
        conn.sendall(b"PONG")
        dut.expect(re.compile(rb"RX PONG"), timeout=60)

        conn.shutdown(socket.SHUT_RDWR)
        conn.close()

        dut.expect(re.compile(rb"CLOSED"), timeout=60)
    finally:
        listener.close()
