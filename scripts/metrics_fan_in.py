#!/usr/bin/env python3
"""Fan-in metrics endpoint for multiple local exporter instances.

Reads comma-separated ports from TARGET_PORTS_CSV and serves a merged
Prometheus exposition format response on PUBLIC_PORT.
"""

from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
from urllib.request import urlopen
import os

TARGET_PORTS = [p.strip() for p in os.getenv("TARGET_PORTS_CSV", "").split(",") if p.strip()]
PUBLIC_PORT = int(os.getenv("PUBLIC_PORT", "9157"))


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/metrics":
            self.send_response(404)
            self.end_headers()
            return

        seen_meta = set()
        merged = []

        for port in TARGET_PORTS:
            try:
                with urlopen(f"http://127.0.0.1:{port}/metrics", timeout=3) as resp:
                    body = resp.read().decode("utf-8", errors="replace")
                for line in body.splitlines():
                    if line.startswith("# HELP") or line.startswith("# TYPE") or line.startswith("# UNIT"):
                        if line in seen_meta:
                            continue
                        seen_meta.add(line)
                    merged.append(line)
            except Exception as exc:
                merged.append(f"# fan_in_error{{port=\"{port}\"}} {repr(exc)}")

        payload = ("\n".join(merged) + "\n").encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        return


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


if __name__ == "__main__":
    server = ThreadedHTTPServer(("0.0.0.0", PUBLIC_PORT), Handler)
    server.serve_forever()
