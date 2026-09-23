# ============================================================================
#  Native Engine — server.py
#  Built-in zero-dependency live preview server. Streams the ray-traced frame
#  as a PNG (multipart) with a small HTML viewer overlaying engine stats.
# ============================================================================
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

_VIEWER_HTML = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Native Engine — Live RT Preview</title>
<style>
  html, body { margin:0; height:100%; background:#0b0d12; color:#cfd8e3;
               font: 13px/1.5 ui-monospace, Menlo, Consolas, monospace; }
  #wrap { display:flex; flex-direction:column; height:100%; }
  #bar { padding:8px 14px; display:flex; gap:22px; align-items:baseline;
         border-bottom:1px solid #1c2230; background:#10131b; }
  #bar b { color:#fff; letter-spacing:.5px; }
  .k { color:#5b6b81; }
  #stage { flex:1; display:flex; align-items:center; justify-content:center;
           min-height:0; }
  img { max-width:100%; max-height:100%; image-rendering:auto;
        box-shadow:0 0 60px rgba(0,0,0,.6); }
</style>
</head>
<body>
<div id="wrap">
  <div id="bar">
    <b>%TITLE%</b>
    <span><span class="k">res</span> %RES%</span>
    <span><span class="k">spp/frame</span> %SPP%</span>
    <span><span class="k">ms</span> <span id="ms">–</span></span>
    <span><span class="k">Mrays/s</span> <span id="mr">–</span></span>
    <span><span class="k">threads</span> %THREADS%</span>
    <span style="margin-left:auto"><span class="k">status</span> <span id="st">connecting…</span></span>
  </div>
  <div id="stage"><img id="frame" alt="render"></div>
</div>
<script>
const img = document.getElementById('frame');
const ms = document.getElementById('ms'), mr = document.getElementById('mr'),
      st = document.getElementById('st');
img.onload = () => { st.textContent = 'live'; };
img.onerror = () => { st.textContent = 'stream interrupted'; };
(function pump() {
  const t0 = performance.now();
  fetch('/frame.png?ts=' + Date.now())
    .then(r => { if (!r.ok) throw 0; return r.blob(); })
    .then(b => {
      img.src = URL.createObjectURL(b);
      const dt = performance.now() - t0;
      ms.textContent = dt.toFixed(0);
      return fetch('/stats.json');
    })
    .then(r => r.json())
    .then(s => { mr.textContent = s.mrays.toFixed(1); })
    .catch(() => { st.textContent = 'reconnecting…'; })
    .finally(() => setTimeout(pump, %INTERVAL%));
})();
</script>
</body>
</html>
"""


def start(app, spp=1, interval_ms=60, host="0.0.0.0", port=0, title="Native Engine"):
    """
    Start serving a live preview of `app` in a daemon thread.

    Every request to /frame.png re-renders the current scene (spp per frame),
    so Python-side animation (camera orbit, moving objects) just works.

    Returns (server, port).
    """
    state = {"frames": 0}
    render_lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *a):  # silence
            pass

        def _send(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = self.path.split("?")[0]
            if path == "/" or path == "/index.html":
                html = (_VIEWER_HTML
                        .replace("%TITLE%", title)
                        .replace("%RES%", f"{app.width}x{app.height}")
                        .replace("%SPP%", str(spp))
                        .replace("%THREADS%", str(app.thread_count))
                        .replace("%INTERVAL%", str(interval_ms)))
                self._send(200, "text/html; charset=utf-8", html.encode())
            elif path == "/frame.png":
                with render_lock:
                    app.render(spp)
                    png = app.frame_png()
                state["frames"] += 1
                self._send(200, "image/png", png)
            elif path == "/stats.json":
                body = (b'{"ms": ' + str(round(app.last_frame_ms, 1)).encode() +
                        b', "mrays": ' + str(round(app.last_mrays_per_sec, 2)).encode() +
                        b', "samples": ' + str(app.accumulated_samples).encode() +
                        b', "frames": ' + str(state["frames"]).encode() + b'}')
                self._send(200, "application/json", body)
            elif path == "/healthz":
                self._send(200, "text/plain", b"ok")
            else:
                self._send(404, "text/plain", b"not found")

    server = ThreadingHTTPServer((host, port), Handler)
    server.daemon_threads = True
    actual_port = server.server_address[1]
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    return server, actual_port
