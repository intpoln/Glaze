"""Local receiver for PNGs rendered in Pocket Texture Lab (browser pane -> disk).

GET  /            serves Tools/texgen/ptl/texture-lab.html (same origin, so fetch() to /save is allowed).
POST /save?path=<relative/path.png>  body: raw PNG bytes. Files land under OUT_DIR.
Run: python Tools/texgen/receiver.py <out_dir> [port]
"""
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

OUT_DIR = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "texgen_out")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8765


class Handler(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        # "/" -> the lab page; "/<name>.js" -> a script from Tools/texgen (material recipes).
        here = os.path.dirname(os.path.abspath(__file__))
        name = urlparse(self.path).path.lstrip("/")
        page = os.path.join(here, name) if name.endswith(".js") and "/" not in name else os.path.join(here, "ptl", "texture-lab.html")
        if not os.path.isfile(page):
            self.send_response(404)
            self.end_headers()
            return
        with open(page, "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", "text/javascript; charset=utf-8" if page.endswith(".js") else "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        query = parse_qs(urlparse(self.path).query)
        rel = query.get("path", [""])[0].replace("\\", "/")
        target = os.path.abspath(os.path.join(OUT_DIR, rel))
        if not rel.endswith((".png", ".json")) or not target.startswith(OUT_DIR + os.sep):
            self.send_response(400)
            self._cors()
            self.end_headers()
            return
        data = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as f:
            f.write(data)
        self.send_response(200)
        self._cors()
        self.end_headers()
        self.wfile.write(str(len(data)).encode())

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"receiver: {OUT_DIR} on 127.0.0.1:{PORT}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
