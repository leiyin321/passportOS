#!/usr/bin/env python3
"""Serve a staged Passport OS release over loopback for local device testing.

Place this HTTP server behind an HTTPS tunnel (for example, cloudflared Quick
Tunnel). The device still validates the tunnel's public TLS certificate and
the signed firmware manifest.
"""
import argparse
import json
import re
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class UpdateHandler(SimpleHTTPRequestHandler):
    server_version = "PassportUpdateTest/1"

    def __init__(self, *args, directory, **kwargs):
        super().__init__(*args, directory=str(directory), **kwargs)

    def do_GET(self):
        if self.path == "/" or self.path == "/latest.json":
            return super().do_GET() if self.path == "/latest.json" else self.send_error(404)
        if not re.fullmatch(r"/passport-os-[0-9]+\.[0-9]+\.[0-9]+\.bin", self.path):
            return self.send_error(404)
        return super().do_GET()

    def list_directory(self, path):
        return self.send_error(404)

    def copyfile(self, source, outputfile):
        copied = 0
        try:
            while True:
                block = source.read(64 * 1024)
                if not block:
                    break
                outputfile.write(block)
                copied += len(block)
        finally:
            print(f"[update-http] response bytes={copied}", flush=True)

    def log_message(self, fmt, *args):
        print("[update-http] " + fmt % args, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=Path("build/system-updates/local-update"))
    parser.add_argument("--host", default="127.0.0.1", help="bind address; use the Mac LAN IP for device tests")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    root = args.directory.resolve()
    manifest_path = root / "latest.json"
    manifest = json.loads(manifest_path.read_text())
    image = manifest.get("image")
    if not isinstance(image, str) or not re.fullmatch(r"passport-os-[0-9]+\.[0-9]+\.[0-9]+\.bin", image):
        raise SystemExit("latest.json has an invalid firmware image name")
    if not (root / image).is_file():
        raise SystemExit(f"release image missing: {root / image}")
    handler = lambda *a, **kw: UpdateHandler(*a, directory=root, **kw)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"Serving {manifest['version']} build {manifest['build']} from {root} at http://{args.host}:{args.port}/latest.json", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
