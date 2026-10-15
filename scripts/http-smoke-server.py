#!/usr/bin/env python3
"""Tiny deterministic HTTP/1.0 fixture for the QEMU guest's built-in browser smoke test."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import argparse
from pathlib import Path

PAGE = (
    b"<!doctype html><html><head><title>Fluxmax smoke</title></head><body>"
    b"<h1>Fluxmax HTTP browser smoke page</h1>"
    b"<p>HTTP fetch succeeded through the independent TCP stack.</p>"
    b"<script>this_text_must_not_be_rendered()</script>"
    b"</body></html>"
)

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
    def do_GET(self):
        if self.path != "/":
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(PAGE)
    def log_message(self, fmt, *args):
        print("http-smoke:", fmt % args, flush=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--port-file", type=Path)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    if args.port_file:
        args.port_file.write_text(str(server.server_address[1]), encoding="ascii")
    print("HTTP smoke fixture listening on", server.server_address[1], flush=True)
    server.serve_forever()
