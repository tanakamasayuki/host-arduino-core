"""HTTPClient over plain HTTP — covers Content-Length and chunked
response paths against a local Python http.server.
"""

import re
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer


BODY_TEXT = b"hello-from-host-arduino-core"
CHUNK_PARTS = [b"chunk-one;", b"chunk-two"]
CHUNK_JOINED = b"".join(CHUNK_PARTS)


class _Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/chunked"):
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for part in CHUNK_PARTS:
                self.wfile.write(f"{len(part):x}\r\n".encode())
                self.wfile.write(part)
                self.wfile.write(b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(BODY_TEXT)))
        self.end_headers()
        self.wfile.write(BODY_TEXT)

    def log_message(self, *args, **kwargs):  # silence default access logging
        pass


def _start_server() -> HTTPServer:
    server = HTTPServer(("127.0.0.1", 0), _Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def test_http_get(dut):
    server = _start_server()
    try:
        dut.expect(re.compile(rb"READY"), timeout=10)

        # 1. Content-Length response
        dut.write(f"GET http://127.0.0.1:{server.server_port}/hello\n".encode())
        dut.expect(re.compile(rb"CODE=200"), timeout=10)
        dut.expect(re.compile(rb"LEN=" + str(len(BODY_TEXT)).encode()), timeout=5)
        dut.expect(re.compile(rb"BODY_LEN=" + str(len(BODY_TEXT)).encode()), timeout=5)
        dut.expect(re.compile(rb"BODY:" + re.escape(BODY_TEXT)), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)

        # 2. Transfer-Encoding: chunked response
        dut.write(f"GET http://127.0.0.1:{server.server_port}/chunked\n".encode())
        dut.expect(re.compile(rb"CODE=200"), timeout=10)
        # LEN is -1 for chunked since no Content-Length header
        dut.expect(re.compile(rb"LEN=-1"), timeout=5)
        dut.expect(re.compile(rb"BODY_LEN=" + str(len(CHUNK_JOINED)).encode()), timeout=5)
        dut.expect(re.compile(rb"BODY:" + re.escape(CHUNK_JOINED)), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)
    finally:
        server.shutdown()
        server.server_close()
