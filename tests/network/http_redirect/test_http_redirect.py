"""HTTPClient redirect following — covers DISABLE / STRICT / FORCE modes,
multi-hop chains, root-relative Location, and the redirect-limit cap.
"""

import re
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer


FINAL_BODY = b"final-payload"


class _Handler(BaseHTTPRequestHandler):
    def _redirect(self, location: str, code: int = 302):
        self.send_response(code)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        # /once    -> 302 -> /target
        # /chain/N -> 302 -> /chain/N-1 (root-relative), N==0 returns body
        # /target  -> 200 body
        # /loop    -> 302 -> /loop (infinite, exercises limit)
        if self.path == "/once":
            self._redirect("/target")
            return
        if self.path == "/target":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(FINAL_BODY)))
            self.end_headers()
            self.wfile.write(FINAL_BODY)
            return
        if self.path.startswith("/chain/"):
            try:
                n = int(self.path[len("/chain/"):])
            except ValueError:
                self.send_response(400); self.end_headers(); return
            if n <= 0:
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(FINAL_BODY)))
                self.end_headers()
                self.wfile.write(FINAL_BODY)
                return
            self._redirect(f"/chain/{n - 1}")
            return
        if self.path == "/loop":
            self._redirect("/loop")
            return
        self.send_response(404); self.end_headers()

    def log_message(self, *a, **kw):
        pass


def _start_server() -> HTTPServer:
    server = HTTPServer(("127.0.0.1", 0), _Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def test_http_redirect(dut):
    server = _start_server()
    port = server.server_port
    base = f"http://127.0.0.1:{port}"
    try:
        dut.expect(re.compile(rb"READY"), timeout=30)

        # 1. DISABLE: 302 surfaces as the status code; Location header is exposed.
        dut.write(b"MODE 0\n")
        dut.expect(re.compile(rb"MODE_OK 0"), timeout=5)
        dut.write(f"GET {base}/once\n".encode())
        dut.expect(re.compile(rb"CODE=302"), timeout=30)
        dut.expect(re.compile(rb"LOC=/target"), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)

        # 2. STRICT: GET is followed; body of /target is delivered.
        dut.write(b"MODE 1\n")
        dut.expect(re.compile(rb"MODE_OK 1"), timeout=5)
        dut.write(f"GET {base}/once\n".encode())
        dut.expect(re.compile(rb"CODE=200"), timeout=30)
        dut.expect(re.compile(rb"BODY:" + re.escape(FINAL_BODY)), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)

        # 3. FORCE: 4-hop chain via root-relative Locations resolves to body.
        dut.write(b"MODE 2\n")
        dut.expect(re.compile(rb"MODE_OK 2"), timeout=5)
        dut.write(f"GET {base}/chain/4\n".encode())
        dut.expect(re.compile(rb"CODE=200"), timeout=15)
        dut.expect(re.compile(rb"BODY:" + re.escape(FINAL_BODY)), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)

        # 4. Redirect limit cap: /loop redirects to itself. With
        #    limit=2 we issue 3 requests (initial + 2 follow-ups), each
        #    a 302; the caller sees the last 302 + the unresolved
        #    Location header.
        dut.write(b"LIMIT 2\n")
        dut.expect(re.compile(rb"LIMIT_OK 2"), timeout=5)
        dut.write(f"GET {base}/loop\n".encode())
        dut.expect(re.compile(rb"CODE=302"), timeout=15)
        dut.expect(re.compile(rb"LOC=/loop"), timeout=5)
        dut.expect(re.compile(rb"DONE"), timeout=5)
    finally:
        server.shutdown()
        server.server_close()
