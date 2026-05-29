"""HTTPClient over TLS — confirms that the internal WiFiClientSecure
path works end-to-end with a self-signed local server.

Requires:
  * tls=openssl board menu option (set via sketch.yaml).
  * `openssl` CLI for ephemeral cert generation.
  * libssl-dev installed so OpenSSL is linkable.
"""

import re
import ssl
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path


BODY_TEXT = b"hello-over-tls"


class _Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(BODY_TEXT)))
        self.end_headers()
        self.wfile.write(BODY_TEXT)

    def log_message(self, *args, **kwargs):
        pass


def _make_self_signed_cert(tmpdir: Path) -> tuple[Path, Path]:
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


def test_https_get(dut, tmp_path):
    cert_path, key_path = _make_self_signed_cert(tmp_path)

    server = HTTPServer(("127.0.0.1", 0), _Handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile=str(cert_path), keyfile=str(key_path))
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        dut.expect(re.compile(rb"READY"), timeout=10)
        dut.write(f"GET https://127.0.0.1:{server.server_port}/hello\n".encode())
        dut.expect(re.compile(rb"CODE=200"), timeout=10)
        dut.expect(re.compile(rb"LEN=" + str(len(BODY_TEXT)).encode()), timeout=10)
        dut.expect(re.compile(rb"BODY_LEN=" + str(len(BODY_TEXT)).encode()), timeout=10)
        dut.expect(re.compile(rb"BODY:" + re.escape(BODY_TEXT)), timeout=10)
        dut.expect(re.compile(rb"DONE"), timeout=10)
    finally:
        server.shutdown()
        server.server_close()
