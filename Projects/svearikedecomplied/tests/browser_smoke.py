#!/usr/bin/env python3
"""Run the actual Canvas/Wasm UI in headless Firefox and compare native pixels."""
import functools
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]


def main():
    result = []
    done = threading.Event()
    # Generate fresh expectations from the current native build and artwork.
    # Reusing analysis/previews can silently compare against stale snapshots.
    with tempfile.TemporaryDirectory(prefix='svea-native-') as snapshots:
        subprocess.run([str(ROOT/'build/svea-menu-probe'),str(ROOT/'build/wasm/menu.pack'),snapshots],check=True)
        expected = {p.stem: hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in Path(snapshots).glob('*.rgba')}

    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            if self.path == '/expected.json':
                data = json.dumps(expected).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(data)
            else:
                super().do_GET()

        def do_POST(self):
            if self.path != '/result':
                self.send_error(404)
                return
            length = int(self.headers['Content-Length'])
            result.append(json.loads(self.rfile.read(length)))
            self.send_response(200)
            self.end_headers()
            done.set()

    server = ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(ROOT)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f'http://127.0.0.1:{server.server_port}/tests/browser_test.html'
    try:
        with tempfile.TemporaryDirectory(prefix='svea-firefox-') as profile:
            with (ROOT/'analysis/browser-firefox.log').open('wb') as log:
                process = subprocess.Popen(['firefox', '--headless', '--no-remote', '--profile', profile, url],
                                           stdout=log, stderr=log)
                try:
                    # Three crossbow routes, Linné, Swedish artillery/retreat and
                    # a complete manual defeat include original animation waits.
                    if not done.wait(300):
                        raise RuntimeError('Firefox test timed out; see analysis/browser-firefox.log')
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
    finally:
        server.shutdown()
        server.server_close()
    report = result[0]
    (ROOT/'analysis/browser-test.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    if not report.get('passed'):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
