#!/usr/bin/env python3
"""UDP Monitor GUI — browser-based UI, no external dependencies"""

import socket
import struct
import time
import os
import csv
import json
import threading
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
from collections import OrderedDict
from urllib.request import urlopen
from concurrent.futures import ThreadPoolExecutor, as_completed

MCAST_GRP = '239.255.255.250'
MCAST_PORT = 1999
HTTP_PORT = 8199

# Global state
scan_state = {
    'scanning': False,
    'progress': 0,
    'status': 'Ready',
    'packets': 0,
    'results': [],
    'clashes': [],
    'summary': '',
    'stop_flag': False,
}

HTML_PAGE = '''<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>UDP Monitor - Tasmota</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
       background: #1a1a2e; color: #e0e0e0; padding: 20px; }
h1 { color: #1fa3ec; margin-bottom: 15px; font-size: 1.5em; }
.controls { display: flex; align-items: center; gap: 15px; margin-bottom: 15px;
            background: #16213e; padding: 12px 18px; border-radius: 8px; }
label { font-size: 0.9em; color: #aaa; }
input[type=number] { width: 70px; padding: 6px 10px; border: 1px solid #333;
                     border-radius: 4px; background: #0f3460; color: #fff;
                     font-size: 1em; }
button { padding: 8px 24px; border: none; border-radius: 6px; font-size: 1em;
         cursor: pointer; font-weight: 600; transition: all 0.2s; }
.btn-start { background: #1fa3ec; color: #fff; }
.btn-start:hover { background: #0e70a4; }
.btn-stop { background: #d43535; color: #fff; }
.btn-stop:hover { background: #931f1f; }
.btn-export { background: #47c266; color: #fff; }
.btn-export:hover { background: #5aaf6f; }
.btn-export:disabled { background: #333; color: #666; cursor: default; }
.btn-exit { background: #c83c3c; color: #fff; margin-left: auto; font-weight: 600; }
.btn-exit:hover { background: #d44848; }
.controls { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; flex-wrap: wrap; }
.progress-bar { height: 6px; background: #16213e; border-radius: 3px;
                margin-bottom: 10px; overflow: hidden; }
.progress-fill { height: 100%; background: #1fa3ec; transition: width 0.5s; width: 0%; }
.status { font-size: 0.85em; color: #888; margin-bottom: 12px; }
table { width: 100%; border-collapse: collapse; font-size: 0.85em; }
th { background: #16213e; color: #1fa3ec; padding: 8px 12px; text-align: left;
     position: sticky; top: 0; cursor: pointer; user-select: none; }
th:hover { background: #1a2a4e; }
td { padding: 6px 12px; border-bottom: 1px solid #1a1a2e; }
tr { background: #0f3460; }
tr:hover { background: #1a3a6e; }
tr.group { background: #16213e; font-weight: 600; color: #1fa3ec; }
tr.clash td { background: #3d1515; }
.clash-box { background: #3d1515; border: 1px solid #d43535; border-radius: 6px;
             padding: 12px; margin-top: 15px; }
.clash-box h3 { color: #d43535; margin-bottom: 8px; }
.summary { margin-top: 12px; font-size: 0.9em; color: #aaa; }
.table-wrap { max-height: calc(100vh - 220px); overflow-y: auto; border-radius: 8px; }
</style>
</head><body>
<h1>UDP Monitor &mdash; Tasmota Multicast Variables</h1>
<div class="controls">
  <label>Scan duration (s):</label>
  <input type="number" id="duration" value="60" min="10" max="300" step="10">
  <button id="scanBtn" class="btn-start" onclick="toggleScan()">Start Scan</button>
  <button id="exportBtn" class="btn-export" onclick="exportCSV()" disabled>Export CSV</button>
  <span class="status" id="status">Ready</span>
  <button id="exitBtn" class="btn-exit" onclick="exitServer()" title="Stop the server and close the window">&times; Exit</button>
</div>
<div class="progress-bar"><div class="progress-fill" id="progress"></div></div>
<div class="table-wrap">
<table id="results"><thead>
<tr><th onclick="sortBy(0)">Sender IP</th><th onclick="sortBy(1)">Device</th>
<th onclick="sortBy(2)">Variable</th><th onclick="sortBy(3)">Type</th>
<th onclick="sortBy(4)">Value</th><th onclick="sortBy(5)">Count</th>
<th onclick="sortBy(6)">Clash</th></tr>
</thead><tbody id="tbody"></tbody></table>
</div>
<div id="clashBox" style="display:none" class="clash-box"></div>
<div class="summary" id="summary"></div>

<script>
let polling = false;
function toggleScan() {
  if (polling) {
    fetch('/api/stop').then(() => { polling = false; });
  } else {
    let dur = document.getElementById('duration').value;
    fetch('/api/start?duration=' + dur).then(() => {
      polling = true;
      document.getElementById('scanBtn').textContent = 'Stop';
      document.getElementById('scanBtn').className = 'btn-stop';
      document.getElementById('exportBtn').disabled = true;
      document.getElementById('tbody').innerHTML = '';
      document.getElementById('clashBox').style.display = 'none';
      document.getElementById('summary').textContent = '';
      poll();
    });
  }
}

function poll() {
  if (!polling) return;
  fetch('/api/status').then(r => r.json()).then(d => {
    document.getElementById('progress').style.width = d.progress + '%';
    document.getElementById('status').textContent = d.status;
    if (!d.scanning && d.results.length > 0) {
      polling = false;
      document.getElementById('scanBtn').textContent = 'Start Scan';
      document.getElementById('scanBtn').className = 'btn-start';
      document.getElementById('exportBtn').disabled = false;
      showResults(d);
    } else if (!d.scanning) {
      polling = false;
      document.getElementById('scanBtn').textContent = 'Start Scan';
      document.getElementById('scanBtn').className = 'btn-start';
      document.getElementById('status').textContent = d.status || 'Done';
    } else {
      setTimeout(poll, 500);
    }
  });
}

function showResults(d) {
  let tb = document.getElementById('tbody');
  tb.innerHTML = '';
  let lastIp = '';
  d.results.forEach(r => {
    if (r[0] !== lastIp) {
      let gr = document.createElement('tr');
      gr.className = 'group';
      gr.innerHTML = '<td>' + r[0] + '</td><td colspan="6">' + (r[1]||'') +
                     ' &mdash; ' + r[7] + '</td>';
      tb.appendChild(gr);
      lastIp = r[0];
    }
    let tr = document.createElement('tr');
    if (r[6]) tr.className = 'clash';
    tr.innerHTML = '<td>' + r[0] + '</td><td>' + r[1] + '</td><td>' + r[2] +
                   '</td><td>' + r[3] + '</td><td>' + r[4] + '</td><td>' + r[5] +
                   '</td><td>' + (r[6] ? 'YES' : '') + '</td>';
    tb.appendChild(tr);
  });
  if (d.clashes.length > 0) {
    let box = document.getElementById('clashBox');
    box.style.display = 'block';
    box.innerHTML = '<h3>Name Clashes (' + d.clashes.length + ')</h3>' +
      d.clashes.map(c => '<p>' + c + '</p>').join('');
  }
  document.getElementById('summary').textContent = d.summary;
}

function exportCSV() { window.open('/api/csv'); }

function exitServer() {
  if (!confirm('Stop the UDP Monitor server?')) return;
  // Stop any running scan first (cosmetic — the server will exit anyway)
  fetch('/api/stop').catch(() => {});
  fetch('/api/shutdown').catch(() => {});
  document.body.innerHTML =
    '<div style="font:1.2em sans-serif;padding:60px;text-align:center;color:#888;background:#1e1e1e;height:100vh;margin:0;">' +
    '<h2 style="color:#fff">UDP Monitor stopped.</h2>' +
    '<p>You can close this tab.</p></div>';
}

function sortBy(col) {
  let tb = document.getElementById('tbody');
  let rows = Array.from(tb.querySelectorAll('tr:not(.group)'));
  rows.sort((a, b) => {
    let va = a.cells[col]?.textContent || '';
    let vb = b.cells[col]?.textContent || '';
    if (col === 5) return parseInt(vb||0) - parseInt(va||0);
    return va.localeCompare(vb);
  });
  // Rebuild with group headers
  let groups = new Map();
  rows.forEach(r => {
    let ip = r.cells[0].textContent;
    if (!groups.has(ip)) groups.set(ip, []);
    groups.get(ip).push(r);
  });
  tb.innerHTML = '';
  groups.forEach((gRows, ip) => {
    let gh = tb.querySelector('.group[data-ip="'+ip+'"]');
    // find original group header
    let orig = document.createElement('tr');
    orig.className = 'group';
    let dev = gRows[0]?.cells[1]?.textContent || '';
    orig.innerHTML = '<td>'+ip+'</td><td colspan="6">'+dev+'</td>';
    tb.appendChild(orig);
    gRows.forEach(r => tb.appendChild(r));
  });
}
</script>
</body></html>'''


def parse_packet(data):
    text = data.decode('latin-1')
    lines = text.split('\n')
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith('=>'):
            line = line[2:]
        eq = line.find('=')
        col = line.find(':')
        if eq >= 0 and (col < 0 or eq < col):
            name = line[:eq]
            val = line[eq+1:]
            yield (name, val, 'ascii')
        elif col >= 0:
            name = line[:col]
            raw = data[data.find(line.encode('latin-1')) + col + 1:]
            if len(raw) > 4:
                alen = raw[0] | (raw[1] << 8)
                remaining = len(raw) - 2
                if alen > 0 and remaining == alen * 4:
                    floats = []
                    for i in range(alen):
                        f = struct.unpack('<f', raw[2 + i*4 : 2 + i*4 + 4])[0]
                        floats.append(f)
                    val = '[' + ', '.join(f'{f:.2f}' for f in floats[:8])
                    if alen > 8:
                        val += f', ... ({alen} total)'
                    val += ']'
                    yield (name, val, f'bin float[{alen}]')
                    continue
            if len(raw) >= 4:
                f = struct.unpack('<f', raw[:4])[0]
                yield (name, f'{f:.2f}', 'bin float')


def get_device_name(ip):
    try:
        resp = urlopen(f'http://{ip}/cm?cmnd=DeviceName', timeout=5)
        data = json.loads(resp.read())
        return data.get('DeviceName', None)
    except Exception:
        pass
    try:
        resp = urlopen(f'http://{ip}/cm?cmnd=Status%205', timeout=5)
        data = json.loads(resp.read())
        return data.get('StatusNET', {}).get('Hostname', None)
    except Exception:
        return None


def scan_worker(duration):
    global scan_state
    scan_state['scanning'] = True
    scan_state['progress'] = 0
    scan_state['packets'] = 0
    scan_state['results'] = []
    scan_state['clashes'] = []
    scan_state['summary'] = ''
    scan_state['status'] = 'Scanning...'
    scan_state['stop_flag'] = False

    seen = OrderedDict()

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        sock.bind(('', MCAST_PORT))
        mreq = struct.pack('4s4s', socket.inet_aton(MCAST_GRP), socket.inet_aton('0.0.0.0'))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        sock.settimeout(1.0)
    except Exception as e:
        scan_state['status'] = f'Error: {e}'
        scan_state['scanning'] = False
        return

    start = time.time()
    end = start + duration

    while not scan_state['stop_flag'] and time.time() < end:
        elapsed = time.time() - start
        scan_state['progress'] = min(100, int(elapsed / duration * 100))
        remaining = max(0, int(end - time.time()))
        scan_state['status'] = f'Scanning... {remaining}s left, {scan_state["packets"]} packets'
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        scan_state['packets'] += 1
        src = addr[0]
        now = time.time()
        for name, val, ptype in parse_packet(data):
            key = (src, name)
            if key in seen:
                seen[key]['value'] = val
                seen[key]['timestamps'].append(now)
            else:
                seen[key] = {'name': name, 'value': val, 'source': src,
                             'type': ptype, 'timestamps': [now]}

    sock.close()

    if not seen:
        scan_state['status'] = 'No variables received'
        scan_state['scanning'] = False
        return

    # Resolve device names
    scan_state['status'] = 'Resolving device names...'
    sender_ips = sorted(set(info['source'] for info in seen.values()),
                        key=lambda ip: tuple(int(p) for p in ip.split('.')))
    device_names = {}
    with ThreadPoolExecutor(max_workers=8) as pool:
        futures = {pool.submit(get_device_name, ip): ip for ip in sender_ips}
        for future in as_completed(futures):
            ip = futures[future]
            name = future.result()
            if name:
                device_names[ip] = name

    # Calculate intervals
    device_timestamps = {}
    for key, info in seen.items():
        src = info['source']
        if src not in device_timestamps:
            device_timestamps[src] = []
        device_timestamps[src].extend(info['timestamps'])

    device_intervals = {}
    for src, ts in device_timestamps.items():
        ts.sort()
        if len(ts) >= 2:
            diffs = [ts[i+1] - ts[i] for i in range(len(ts)-1)]
            burst_gaps = [d for d in diffs if d > 0.3]
            if burst_gaps:
                device_intervals[src] = f"{sum(burst_gaps)/len(burst_gaps):.1f}s"
            else:
                device_intervals[src] = "cont."
        else:
            device_intervals[src] = "once"

    # Detect clashes
    name_senders = {}
    for key, info in seen.items():
        vname = info['name']
        if vname not in name_senders:
            name_senders[vname] = set()
        name_senders[vname].add(info['source'])
    clashes = {n: s for n, s in name_senders.items() if len(s) > 1}

    # Build results
    def ip_sort_key(item):
        info = item[1]
        parts = info['source'].split('.')
        return (tuple(int(p) for p in parts), info['name'])

    sorted_items = sorted(seen.items(), key=ip_sort_key)
    results = []
    for key, info in sorted_items:
        src = info['source']
        name = info['name']
        dname = device_names.get(src, '')
        interval = device_intervals.get(src, '?')
        count = len(info['timestamps'])
        clash = name in clashes
        var_count = sum(1 for _, i in seen.items() if i['source'] == src)
        group_info = f"{var_count} vars, every {interval}"
        results.append([src, dname, name, info['type'], info['value'], count, clash, group_info])

    clash_msgs = []
    for vname, senders in sorted(clashes.items()):
        ips_str = ', '.join(f"{ip} ({device_names.get(ip, '?')})"
                           for ip in sorted(senders, key=lambda ip: tuple(int(p) for p in ip.split('.'))))
        clash_msgs.append(f"'{vname}' sent by: {ips_str}")

    total_vars = len(seen)
    total_devices = len(device_timestamps)
    clash_count = len(clashes)
    clash_msg = f"  |  {clash_count} name clash(es)!" if clash_count else ""

    scan_state['results'] = results
    scan_state['clashes'] = clash_msgs
    scan_state['summary'] = f"{total_vars} variables from {total_devices} devices{clash_msg}"
    scan_state['progress'] = 100
    scan_state['status'] = 'Scan complete'
    scan_state['scanning'] = False

    # Store for CSV export
    scan_state['_sorted'] = sorted_items
    scan_state['_seen'] = seen
    scan_state['_device_names'] = device_names
    scan_state['_device_intervals'] = device_intervals
    scan_state['_clashes'] = clashes


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # suppress log noise

    def do_GET(self):
        if self.path == '/' or self.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode())

        elif self.path.startswith('/api/start'):
            dur = 60
            if '?' in self.path:
                params = dict(p.split('=') for p in self.path.split('?')[1].split('&') if '=' in p)
                dur = int(params.get('duration', 60))
            if not scan_state['scanning']:
                t = threading.Thread(target=scan_worker, args=(dur,), daemon=True)
                t.start()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"ok":true}')

        elif self.path == '/api/stop':
            scan_state['stop_flag'] = True
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"ok":true}')

        elif self.path == '/api/status':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            resp = {
                'scanning': scan_state['scanning'],
                'progress': scan_state['progress'],
                'status': scan_state['status'],
                'packets': scan_state['packets'],
                'results': scan_state['results'],
                'clashes': scan_state['clashes'],
                'summary': scan_state['summary'],
            }
            self.wfile.write(json.dumps(resp).encode())

        elif self.path == '/api/shutdown':
            # Clean exit triggered from the browser "× Exit" button.
            # Reply 200 so the JS keep-alive doesn't show an error,
            # then schedule os._exit so the response actually flushes
            # before the process disappears.
            scan_state['stop_flag'] = True
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"ok":true}')
            import os as _os
            threading.Timer(0.3, lambda: _os._exit(0)).start()

        elif self.path == '/api/csv':
            self.send_response(200)
            self.send_header('Content-Type', 'text/csv; charset=utf-8')
            self.send_header('Content-Disposition', 'attachment; filename="udp_variables.csv"')
            self.end_headers()
            import io
            buf = io.StringIO()
            w = csv.writer(buf)
            w.writerow(["Sender IP", "Device Name", "Variable", "Type", "Value",
                         "Send Interval", "Receive Count", "Name Clash"])
            for r in scan_state.get('results', []):
                w.writerow([r[0], r[1], r[2], r[3], r[4], r[7], r[5],
                           "YES" if r[6] else ""])
            self.wfile.write(buf.getvalue().encode('utf-8-sig'))

        else:
            self.send_response(404)
            self.end_headers()


def main():
    server = HTTPServer(('127.0.0.1', HTTP_PORT), Handler)
    url = f'http://127.0.0.1:{HTTP_PORT}'
    print(f"UDP Monitor running at {url}")

    # Open browser after short delay
    threading.Timer(0.5, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()

if __name__ == '__main__':
    main()
