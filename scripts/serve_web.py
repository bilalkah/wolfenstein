#!/usr/bin/env python3
"""Serves the web build to this machine and the local network.

Like `python3 -m http.server`, plus byte ranges: the page resumes a download
that stalls (see web/shell.html) from the byte it stopped at, instead of
starting the file over.

Usage: serve_web.py <directory> [port]
"""

import functools
import http.server
import os
import re
import socket
import sys

RANGE = re.compile(r"bytes=(\d+)-(\d*)$")


class Handler(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def send_head(self):
        # One handler serves every request of a kept-alive connection
        self.remaining = None
        match = RANGE.match(self.headers.get("Range", ""))
        path = self.translate_path(self.path)
        if not match or not os.path.isfile(path):
            return super().send_head()
        size = os.path.getsize(path)
        start = int(match.group(1))
        end = min(int(match.group(2) or size - 1), size - 1)
        if start > end:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return None
        file = open(path, "rb")
        file.seek(start)
        self.remaining = end - start + 1
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(self.remaining))
        self.end_headers()
        return file

    def copyfile(self, source, outputfile):
        remaining = self.remaining
        if remaining is None:
            return super().copyfile(source, outputfile)
        while remaining > 0:
            chunk = source.read(min(remaining, 64 * 1024))
            if not chunk:
                break
            outputfile.write(chunk)
            remaining -= len(chunk)

    def end_headers(self):
        self.send_header("Accept-Ranges", "bytes")
        super().end_headers()


class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        # The page drops a stalled connection on purpose: not an error
        if isinstance(sys.exc_info()[1], ConnectionError):
            return
        super().handle_error(request, client_address)


def lan_address():
    # The address other machines reach this one at: the interface a packet
    # to the outside would leave from (nothing is sent)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        try:
            probe.connect(("192.0.2.1", 9))
            return probe.getsockname()[0]
        except OSError:
            return None


def main():
    directory = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
    handler = functools.partial(Handler, directory=directory)
    with Server(("0.0.0.0", port), handler) as server:
        print(f"Open http://localhost:{port}", flush=True)
        address = lan_address()
        if address:
            print(f"  or from another machine: http://{address}:{port}", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
