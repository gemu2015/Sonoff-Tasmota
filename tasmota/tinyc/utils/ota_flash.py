#!/usr/bin/env python3
"""ota_flash.py — reliable safeboot-aware OTA flasher for Tasmota (CLI).

This is the command-line twin of the Tasmota Workbench's OTA flasher
(tasmota_workbench/tasmota_workbench_server.py :: _flash_ota). Use it from a
build host to push a freshly-built firmware.bin to a device over the LAN.

WHY THIS METHOD (and not POST /u2):
  We do NOT upload the .bin to the device's /u2 web-updater. On the
  single-app + safeboot partition layout (esp32_partition_app1856k_fs1344k),
  pushing a full image to /u2 — even after triggering safeboot via
  /u4?u4=fct — fails with "Not enough space" when the device's *safeboot*
  partition is an old build with a stale partition view.

  Instead we let the DEVICE pull the image itself:
      Backlog OtaUrl http://<our-lan-ip>:<port>/fw.bin ; Upgrade 1
  Tasmota's own OTA logic does the safeboot partition switch internally,
  works for full images, and is language-independent (no /u2 page scraping).
  We serve the .bin from an ephemeral HTTP server bound to 0.0.0.0 so the
  device can reach our LAN IP, then poll Status 2 until the new build is up.
  If the device lands in safeboot recovery, we send one `Restart 1`.

USAGE:
  ota_flash.py <ip> <firmware.bin>
  ota_flash.py <ip> --env tinyc32c3-matter      # locate .pio/build/<env>/firmware.bin
  ota_flash.py 192.168.188.143 --env tinyc32c3-matter [--user admin --pass xxx]

Notes:
  * WebPassword: pass --user/--pass; they are sent as Tasmota /cm query params.
  * The matter fabric, /*.tcb and other LittleFS files survive OTA (only the
    app partition is rewritten).
  * Prints the before/after Version + BuildDateTime so you can confirm the swap.
"""
import sys, os, time, socket, argparse, json, threading, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))


def log(m): print(m, flush=True)


def local_ips():
    ips = []
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None):
            ip = info[4][0]
            if '.' in ip and not ip.startswith('127.'):
                ips.append(ip)
    except Exception:
        pass
    # also the default-route source IP
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80)); ips.append(s.getsockname()[0]); s.close()
    except Exception:
        pass
    return ips


def pick_local_ip(dev_ip):
    sub = dev_ip.rsplit('.', 1)[0] + '.'
    for ip in local_ips():
        if ip.startswith(sub):
            return ip
    return local_ips()[0] if local_ips() else '127.0.0.1'


def cm(host, cmnd, user=None, password=None, timeout=8):
    q = 'cmnd=' + urllib.parse.quote(cmnd)
    if password:
        q = ('user=%s&password=%s&' % (urllib.parse.quote(user or 'admin'),
                                       urllib.parse.quote(password))) + q
    try:
        r = urllib.request.urlopen('http://%s/cm?%s' % (host, q), timeout=timeout)
        return r.status, r.read(4000).decode('utf-8', 'replace')
    except urllib.error.HTTPError as e:
        return e.code, ''
    except Exception as e:
        return None, str(e)


def fwr(host, user=None, password=None):
    """(version, builddatetime) or (None, None)."""
    st, t = cm(host, 'Status 2', user, password, timeout=5)
    try:
        f = json.loads(t)['StatusFWR']; return f.get('Version', ''), f.get('BuildDateTime', '')
    except Exception:
        return None, None


def device_mode(host, user=None, password=None):
    """'normal' | 'safeboot' | 'down'."""
    v, _ = fwr(host, user, password)
    if v:
        return ('safeboot' if ('safeboot' in v.lower() or 'minimal' in v.lower()) else 'normal'), v
    # /cm may answer {"Command":"Unknown"} in safeboot; probe root page
    try:
        body = urllib.request.urlopen('http://%s/' % host, timeout=4).read().decode('utf-8', 'replace')
        if 'safeboot' in body.lower():
            return 'safeboot', ''
    except Exception:
        pass
    return 'down', ''


def make_handler(blob):
    class H(BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def _serve(self, body):
            self.send_response(200)
            self.send_header('Content-Type', 'application/octet-stream')
            self.send_header('Content-Length', str(len(blob)))
            self.end_headers()
            if body:
                self.wfile.write(blob)
                log('  → device pulled firmware (%d B)' % len(blob))

        def do_HEAD(self): self._serve(False)
        def do_GET(self):  log('  → device requesting firmware …'); self._serve(True)
    return H


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ip')
    ap.add_argument('bin', nargs='?', help='path to firmware.bin')
    ap.add_argument('--env', help='locate .pio/build/<env>/firmware.bin')
    ap.add_argument('--user', default='admin')
    ap.add_argument('--pass', dest='password', default=None)
    ap.add_argument('--timeout', type=int, default=240)
    a = ap.parse_args()

    binpath = a.bin or (os.path.join(REPO, '.pio', 'build', a.env, 'firmware.bin') if a.env else None)
    if not binpath or not os.path.isfile(binpath):
        log('ERROR: firmware not found: %s' % binpath); sys.exit(2)
    blob = open(binpath, 'rb').read()
    host = a.ip

    before_v, before_b = fwr(host, a.user, a.password)
    log('Device %s  before: %s  %s' % (host, before_v or '?', before_b or ''))
    log('Firmware: %s (%d B)' % (binpath, len(blob)))

    dev_ip = socket.gethostbyname(host.split(':')[0])
    my_ip = pick_local_ip(dev_ip)
    ThreadingHTTPServer.daemon_threads = True
    srv = ThreadingHTTPServer(('0.0.0.0', 0), make_handler(blob))
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = 'http://%s:%d/fw.bin' % (my_ip, port)
    log('Serving %s' % url)

    st, t = cm(host, 'Backlog OtaUrl %s; Upgrade 1' % url, a.user, a.password, timeout=10)
    if st == 401:
        log('ERROR: WebPassword required/incorrect (pass --user/--pass)'); sys.exit(3)
    if st != 200:
        log('ERROR: /cm Upgrade failed (%s) %s' % (st, t[:120])); sys.exit(3)
    log('Upgrade triggered: %s' % t.strip()[:160])

    deadline = time.time() + a.timeout
    time.sleep(8)
    restart_kicked = False
    back = None
    while time.time() < deadline:
        mode, v = device_mode(host, a.user, a.password)
        if mode == 'normal' and v and v != before_v:
            back = v; break
        if mode == 'normal' and v == before_v:
            # came back but unchanged — give the flash a moment more
            pass
        if mode == 'safeboot' and not restart_kicked:
            log('Device in safeboot (%s) — sending Restart 1 to switch to app0…' % (v or '?'))
            cm(host, 'Restart 1', a.user, a.password, timeout=6)
            restart_kicked = True
            deadline = max(deadline, time.time() + 60)
            time.sleep(8); continue
        time.sleep(5)
    srv.shutdown()

    after_v, after_b = fwr(host, a.user, a.password)
    if after_v and after_b and after_b != before_b:
        log('OTA OK — now: %s  %s%s' % (after_v, after_b,
            '  (recovered via Restart 1)' if restart_kicked else ''))
        sys.exit(0)
    log('OTA UNCONFIRMED — now: %s  %s (before %s). Check the device.'
        % (after_v or '?', after_b or '', before_b or '?'))
    sys.exit(1)


if __name__ == '__main__':
    main()
