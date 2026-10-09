"""Local receiver for PNGs rendered in Pocket Texture Lab (browser pane -> disk).

GET  /             serves ptl/texture-lab.html (same origin, so fetch() to /save is allowed).
GET  /materials.js serves the material recipes from this folder. Nothing else is served.
POST /save?path=<relative/path.png|json>  body: raw bytes. Files land under OUT_DIR.
Only the receiver's own origin (127.0.0.1 / localhost on its port) may POST; no CORS headers are sent.
Run: python Tools/texgen/receiver.py <out_dir> [port]

Setting up from a clean checkout (ptl/ is gitignored):
- Put Pocket Texture Lab (https://github.com/intpoln/PocketTextureLab) into Tools/texgen/ptl/ so that
  Tools/texgen/ptl/texture-lab.html exists (the page header of the copy used so far says v0.7.0; the exact
  commit/revision is unknown). Only that one HTML file is served.
- Some materials in materials.js use indices of PTL's built-in example graphs, so another PTL version may differ.
- Pillow is needed for package_pack.py / contact_sheet.py (version unknown): pip install pillow
- Flow: run this receiver, open http://127.0.0.1:<port>/ in a browser, eval materials.js from the page
  (eval(await (await fetch('/materials.js')).text())), render into TexturePack/raw, then run package_pack.py.
"""
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

OUT_DIR = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "texgen_out")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
HERE = os.path.dirname(os.path.abspath(__file__))
MAX_BODY = 64 * 1024 * 1024
READ_TIMEOUT = 30  # seconds per socket read
# Explicit GET allowlist: route -> (file, content type).
ROUTES = {
    "/": (os.path.join(HERE, "ptl", "texture-lab.html"), "text/html; charset=utf-8"),
    "/materials.js": (os.path.join(HERE, "materials.js"), "text/javascript; charset=utf-8"),
}


class Handler(BaseHTTPRequestHandler):
    timeout = READ_TIMEOUT

    def _reply(self, code, body=b""):
        self.send_response(code)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _own_origin(self):
        port = self.server.server_address[1]
        hosts = (f"127.0.0.1:{port}", f"localhost:{port}")
        if self.headers.get("Host", "") not in hosts:
            return False
        origin = self.headers.get("Origin")
        return origin is None or origin in tuple("http://" + h for h in hosts)

    def do_GET(self):
        route = ROUTES.get(urlparse(self.path).path)
        if route is None or not os.path.isfile(route[0]):
            self._reply(404)
            return
        with open(route[0], "rb") as f:
            data = f.read()
        self.send_response(200)
        self.send_header("Content-Type", route[1])
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/save":
            self._reply(404)
            return
        if not self._own_origin():
            self._reply(403)
            return
        length = self.headers.get("Content-Length", "")
        if not length.isdigit() or int(length) == 0:
            self._reply(411)
            return
        if int(length) > MAX_BODY:
            self._reply(413)
            return
        rel = parse_qs(parsed.query).get("path", [""])[0].replace("\\", "/")
        target = os.path.abspath(os.path.join(OUT_DIR, rel))
        if not rel.endswith((".png", ".json")) or not target.startswith(OUT_DIR + os.sep):
            self._reply(400)
            return
        try:
            data = self.rfile.read(int(length))
        except OSError:  # read timeout or dropped connection
            self.close_connection = True
            return
        if len(data) != int(length):
            self._reply(400)
            return
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as f:
            f.write(data)
        self._reply(200, str(len(data)).encode())

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"receiver: {OUT_DIR} on 127.0.0.1:{PORT}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
