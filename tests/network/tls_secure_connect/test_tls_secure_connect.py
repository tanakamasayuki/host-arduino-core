"""End-to-end WiFiClientSecure test.

Spins up a tiny TLS echo server with an ephemeral self-signed cert (the
sketch always skips cert verification on host, so any cert works) and
asks the sketch to connect, send "PING", and echo whatever comes back.

Requires `openssl` CLI for one-shot cert generation (Linux test
machines that have `libssl-dev` already have it).
"""

import re
import socket
import ssl
import subprocess
import tempfile
import threading
from pathlib import Path


def _make_self_signed_cert(tmpdir: Path) -> tuple[Path, Path]:
    """Generate an ephemeral self-signed cert/key via openssl CLI."""
    cert = tmpdir / "cert.pem"
    key = tmpdir / "key.pem"
    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", str(key), "-out", str(cert),
            "-days", "1", "-nodes",
            "-subj", "/CN=host-arduino-core-test",
        ],
        check=True,
        capture_output=True,
    )
    return cert, key


def test_tls_secure_connect(dut, tmp_path):
    dut.expect(re.compile(rb"READY"), timeout=10)

    cert_path, key_path = _make_self_signed_cert(tmp_path)

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile=str(cert_path), keyfile=str(key_path))

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(10.0)
    server_port = listener.getsockname()[1]

    server_state: dict = {}

    def serve_one():
        try:
            raw, _ = listener.accept()
            raw.settimeout(5.0)
            tls = ctx.wrap_socket(raw, server_side=True)
            payload = b""
            while len(payload) < 4:
                chunk = tls.recv(4 - len(payload))
                if not chunk:
                    break
                payload += chunk
            server_state["received"] = payload
            if payload == b"PING":
                tls.sendall(b"PONG")
            try:
                tls.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            tls.close()
        except Exception as exc:  # noqa: BLE001
            server_state["error"] = exc

    t = threading.Thread(target=serve_one, daemon=True)
    t.start()

    try:
        dut.write(f"CONNECT {server_port}\n".encode())
        dut.expect(re.compile(rb"CONNECTING " + str(server_port).encode()), timeout=10)
        dut.expect(re.compile(rb"CONNECTED"), timeout=10)
        dut.expect(re.compile(rb"WROTE 4"), timeout=10)
        dut.expect(re.compile(rb"RX PONG"), timeout=10)

        t.join(timeout=10)
        assert server_state.get("received") == b"PING", server_state

        dut.expect(re.compile(rb"CLOSED"), timeout=10)
    finally:
        listener.close()
