#!/usr/bin/env python3
# Serial Monitor — a browser-based serial console for ESP devices with a
# LARGE scrollback history so events never scroll away. Built in the same
# manner as the SML emulator: one dependency-light Python server
# (pyserial only) serving an embedded single-page UI.
#
#   Port selector · Baudrate selector · Clear · Save
#
# Run:  python3 serial_monitor_server.py   (opens http://localhost:8124/)
# or double-click "Serial Monitor.app" / "Serial Monitor.command".

import os, sys, json, threading, time, socket, subprocess, webbrowser
from collections import deque
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

HTTP_PORT  = 8124
# Big ring so events do NOT scroll away. ~200k lines ≈ tens of MB RAM;
# override with SERIAL_MONITOR_HISTORY=<lines>.
HISTORY    = int(os.environ.get('SERIAL_MONITOR_HISTORY', '200000'))

state_lock  = threading.Lock()
serial_port = None                       # pyserial Serial or None
log_buf     = deque(maxlen=HISTORY)       # dicts: {seq,t,d,x}
log_seq     = 0                           # monotonic; survives rotation
rx_bytes    = 0
cur_device  = ''
cur_baud    = 0


def _open_url(url):
    try:
        if sys.platform == 'darwin':
            subprocess.Popen(['open', url]); return True
    except Exception:
        pass
    try:
        return webbrowser.open(url)
    except Exception:
        return False


def _line_hex(raw):
    """Hex view of a raw line, only stored when it contains
    non-printable / non-ASCII bytes (keeps memory ~unchanged for
    plain text logs; pure-ASCII hex is derived client-side)."""
    if any((b < 0x20 and b != 0x09) or b >= 0x7f for b in raw):
        return ' '.join(f'{b:02x}' for b in raw)
    return None


def add_line(direction, text, hexs=None):
    """direction: 'rx' | 'info' | 'err'"""
    global log_seq
    with state_lock:
        log_seq += 1
        e = {'seq': log_seq, 't': time.strftime('%H:%M:%S'),
             'd': direction, 'x': text}
        if hexs:
            e['h'] = hexs
        log_buf.append(e)


def list_ports():
    try:
        from serial.tools import list_ports as lp
        out = []
        for p in lp.comports():
            out.append({'device': p.device,
                        'desc': (p.description or '').strip()})
        # Helpful default ordering: usbserial/usbmodem first.
        out.sort(key=lambda d: (('usb' not in d['device'].lower()),
                                 d['device']))
        return out
    except ImportError:
        return []


def _serial_reader():
    """Accumulate bytes, emit a log line per newline; flush a pending
    partial line after a short idle so promptless output still shows."""
    global rx_bytes
    buf = bytearray()
    last = time.time()
    while True:
        with state_lock:
            sp = serial_port
        if sp is None:
            buf.clear()
            time.sleep(0.1)
            continue
        try:
            chunk = sp.read(4096)
        except Exception as e:
            add_line('err', f'[serial read error: {e}]')
            _close_serial()
            continue
        now = time.time()
        if chunk:
            with state_lock:
                rx_bytes += len(chunk)
            buf.extend(chunk)
            while True:
                nl = buf.find(b'\n')
                if nl < 0:
                    break
                raw = bytes(buf[:nl]).rstrip(b'\r')
                del buf[:nl + 1]
                add_line('rx', raw.decode('utf-8', 'replace'),
                         _line_hex(raw))
            last = now
        else:
            # idle: flush a lingering partial line (e.g. a prompt)
            if buf and (now - last) > 0.25:
                pr = bytes(buf)
                add_line('rx', pr.decode('utf-8', 'replace'),
                         _line_hex(pr))
                buf.clear()
            time.sleep(0.02)


def _close_serial(quiet=False):
    global serial_port, cur_device, cur_baud
    with state_lock:
        sp = serial_port
        serial_port = None
        dev = cur_device
        cur_device = ''
        cur_baud = 0
    if sp is not None:
        try:
            sp.close()
        except Exception:
            pass
        if not quiet:
            add_line('info', f'[closed {dev}]')


def _open_serial(device, baud):
    global serial_port, cur_device, cur_baud
    try:
        import serial as pyserial
    except ImportError:
        return False, 'pyserial not installed (pip3 install pyserial)'
    _close_serial(quiet=True)
    try:
        p = pyserial.Serial(port=device, baudrate=int(baud),
                             bytesize=8, parity='N', stopbits=1,
                             timeout=0.05)
    except Exception as e:
        return False, str(e)
    with state_lock:
        serial_port = p
        cur_device = device
        cur_baud = int(baud)
    add_line('info', f'[opened {device} @ {baud} baud]')
    return True, None


HTML = r"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Serial Monitor</title>
<style>
 :root{--bg:#0e1116;--bar:#161b22;--fg:#d6deeb;--mut:#7a8aa0;--acc:#3b82f6;
       --rx:#cde7ff;--info:#9ece6a;--err:#ff6b6b}
 *{box-sizing:border-box}
 body{margin:0;font:14px/1.4 -apple-system,Segoe UI,Roboto,sans-serif;
      background:var(--bg);color:var(--fg);height:100vh;display:flex;
      flex-direction:column}
 #bar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;
      padding:8px 10px;background:var(--bar);border-bottom:1px solid #222}
 select,button,input{font:13px inherit;background:#0e1116;color:var(--fg);
      border:1px solid #30363d;border-radius:5px;padding:6px 9px}
 button{cursor:pointer}
 button:hover{border-color:var(--acc)}
 button.go{background:#1f6feb;border-color:#1f6feb;color:#fff}
 button.stop{background:#aa2b2b;border-color:#aa2b2b;color:#fff}
 #stat{margin-left:auto;color:var(--mut);font-size:12px;white-space:nowrap}
 #dot{display:inline-block;width:8px;height:8px;border-radius:50%;
      background:#555;margin-right:5px;vertical-align:middle}
 #dot.on{background:#3fb950}
 label{color:var(--mut);font-size:12px}
 #log{flex:1;overflow:auto;padding:6px 10px;white-space:pre-wrap;
      word-break:break-word;font:12px/1.45 ui-monospace,Menlo,Consolas,
      monospace}
 .l{display:flex;gap:8px}
 .l .t{color:#586274;flex:none;-webkit-user-select:none;
       user-select:none}
 .rx{color:var(--rx)} .info{color:var(--info)} .err{color:var(--err)}
 #log.notime .t{display:none}
</style></head><body>
<div id="bar">
  <label>Port</label>
  <select id="port" style="min-width:190px"></select>
  <button id="refresh" title="Rescan serial ports">⟳</button>
  <label>Baud</label>
  <select id="baud"></select>
  <button id="conn" class="go">Connect</button>
  <button id="clear">Clear</button>
  <button id="save">Save</button>
  <button id="quit" class="stop" title="Stop the server process">Quit</button>
  <label style="margin-left:6px"><input type="checkbox" id="auto" checked
    style="vertical-align:middle"> autoscroll</label>
  <label><input type="checkbox" id="ts" checked
    style="vertical-align:middle"> time</label>
  <label><input type="checkbox" id="hex"
    style="vertical-align:middle"> hex</label>
  <span id="stat"><span id="dot"></span><span id="msg">idle</span></span>
</div>
<div id="log" class="" tabindex="0"></div>
<script>
const $=s=>document.querySelector(s);
const logEl=$('#log'), portEl=$('#port'), baudEl=$('#baud'),
      connEl=$('#conn'), msgEl=$('#msg'), dotEl=$('#dot');
let connected=false, since=0, total=0, dropped=0, pollTimer=null,
    MAXDOM=50000;   // keep huge but bounded scrollback in the DOM

[300,1200,2400,4800,9600,19200,38400,57600,74880,115200,230400,
 460800,921600,1500000].forEach(b=>{
  const o=document.createElement('option');o.value=b;o.textContent=b;
  if(b===115200)o.selected=true;baudEl.appendChild(o);});

async function loadPorts(){
  try{
    const r=await fetch('/api/ports'); const ps=await r.json();
    const keep=portEl.value;
    portEl.innerHTML='';
    if(!ps.length){
      const o=document.createElement('option');
      o.textContent='(no serial ports — is pyserial installed?)';
      o.value='';portEl.appendChild(o);
    }
    ps.forEach(p=>{const o=document.createElement('option');
      o.value=p.device;
      o.textContent=p.device+(p.desc?'  —  '+p.desc:'');
      portEl.appendChild(o);});
    if(keep)portEl.value=keep;
  }catch(e){
    portEl.innerHTML='';
    const o=document.createElement('option');
    o.textContent='(port list failed: '+e+')';o.value='';
    portEl.appendChild(o);
  }
}

const _enc=new TextEncoder();
function toHex(s){return Array.from(_enc.encode(s),
  b=>b.toString(16).padStart(2,'0')).join(' ');}
function hexMode(){return $('#hex').checked;}
function render(span){
  const h=hexMode();
  span.textContent = h ? (span.dataset.h || toHex(span.dataset.x||''))
                       : (span.dataset.x||'');
}

function setStat(on,t){dotEl.classList.toggle('on',on);msgEl.textContent=t;}

function append(lines){
  if(!lines.length)return;
  const atBottom = logEl.scrollHeight-logEl.scrollTop-logEl.clientHeight<40;
  const frag=document.createDocumentFragment();
  for(const ln of lines){
    const d=document.createElement('div');d.className='l';
    const t=document.createElement('span');t.className='t';t.textContent=ln.t;
    const x=document.createElement('span');x.className=ln.d;
    x.dataset.x=ln.x; if(ln.h)x.dataset.h=ln.h; render(x);
    d.appendChild(t);d.appendChild(x);frag.appendChild(d);
  }
  logEl.appendChild(frag);
  // trim DOM to MAXDOM lines (server keeps the full history for Save)
  let over=logEl.childElementCount-MAXDOM;
  while(over-->0 && logEl.firstChild) logEl.removeChild(logEl.firstChild);
  if($('#auto').checked && atBottom) logEl.scrollTop=logEl.scrollHeight;
}

async function poll(){
  try{
    const r=await fetch('/api/poll?since='+since);
    const j=await r.json();
    since=j.seq; total=j.total;
    connected=j.open;
    setStat(j.open, (j.open?('● connected '+(j.dev||'')+' @ '+j.baud)
                            :'○ disconnected')
                    +'  |  '+total+' lines  '+
                    (j.rx_bytes||0)+' B'+
                    (j.dropped?('  ('+j.dropped+' rolled off)'):''));
    connEl.textContent=j.open?'Disconnect':'Connect';
    connEl.className=j.open?'stop':'go';
    if(j.lines&&j.lines.length) append(j.lines);
  }catch(e){ setStat(false,'server unreachable'); }
}

connEl.onclick=async()=>{
  if(connected){ await fetch('/api/close',{method:'POST'}); }
  else{
    const dev=portEl.value;
    if(!dev){alert('Select a serial port');return;}
    const r=await fetch('/api/open',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({device:dev,baud:+baudEl.value})});
    const j=await r.json();
    if(!j.ok) alert('Open failed: '+(j.error||'unknown'));
  }
  poll();
};
$('#refresh').onclick=loadPorts;
$('#clear').onclick=async()=>{
  await fetch('/api/clear',{method:'POST'});
  logEl.innerHTML=''; since=0;
};
$('#save').onclick=()=>{ window.location='/api/save'; };
$('#ts').onchange=e=>logEl.classList.toggle('notime',!e.target.checked);
$('#hex').onchange=()=>{
  for(const s of logEl.querySelectorAll('.l>span:last-child')) render(s);
};
$('#quit').onclick=async()=>{
  if(!confirm('Stop the Serial Monitor server?'))return;
  try{await fetch('/api/quit',{method:'POST'});}catch(e){}
  if(pollTimer)clearInterval(pollTimer);
  setStat(false,'server stopped — you can close this tab');
  document.title='Serial Monitor (stopped)';
};

loadPorts(); poll(); pollTimer=setInterval(poll,250);
</script></body></html>"""


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):           # silence stock logging
        pass

    def _send(self, code, body, ctype='application/json', extra=None):
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        if extra:
            for k, v in extra.items():
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj):
        self._send(200, json.dumps(obj).encode('utf-8'))

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/' or path == '/index.html':
            self._send(200, HTML.encode('utf-8'),
                       'text/html; charset=utf-8')
        elif path == '/api/ports':
            self._json(list_ports())
        elif path == '/api/poll':
            qs = parse_qs(urlparse(self.path).query)
            since = int((qs.get('since') or ['0'])[0])
            with state_lock:
                seq = log_seq
                oldest = log_buf[0]['seq'] if log_buf else seq + 1
                lines = [e for e in log_buf if e['seq'] > since]
                dropped = max(0, (oldest - 1) - since) if since else 0
                openf = serial_port is not None
                dev, baud, rb, tot = cur_device, cur_baud, rx_bytes, \
                    len(log_buf)
            self._json({'lines': lines, 'seq': seq, 'total': tot,
                        'dropped': dropped, 'open': openf,
                        'dev': dev, 'baud': baud, 'rx_bytes': rb})
        elif path == '/api/save':
            with state_lock:
                txt = ''.join(f"{e['t']} {e['x']}\n" for e in log_buf)
            fn = time.strftime('serial-%Y%m%d-%H%M%S.log')
            self._send(200, txt.encode('utf-8', 'replace'),
                       'text/plain; charset=utf-8',
                       {'Content-Disposition':
                        f'attachment; filename="{fn}"'})
        else:
            self._send(404, b'not found', 'text/plain')

    def do_POST(self):
        path = urlparse(self.path).path
        n = int(self.headers.get('Content-Length', 0) or 0)
        raw = self.rfile.read(n) if n else b''
        try:
            body = json.loads(raw) if raw else {}
        except Exception:
            body = {}
        if path == '/api/open':
            ok, err = _open_serial(body.get('device', ''),
                                   body.get('baud', 115200))
            self._json({'ok': ok, 'error': err})
        elif path == '/api/close':
            _close_serial()
            self._json({'ok': True})
        elif path == '/api/clear':
            with state_lock:
                log_buf.clear()
            self._json({'ok': True})
        elif path == '/api/quit':
            self._json({'ok': True})
            _close_serial(quiet=True)
            # let the response flush, then hard-exit the process
            threading.Timer(0.2, lambda: os._exit(0)).start()
        else:
            self._send(404, b'not found', 'text/plain')


def main():
    threading.Thread(target=_serial_reader, daemon=True).start()
    # Safari resolves "localhost" to IPv6 ::1; bind all interfaces and
    # open via 127.0.0.1 so the page AND its fetch()s reach the server.
    url = f'http://127.0.0.1:{HTTP_PORT}/'
    # If already running, just focus the existing instance.
    try:
        s = socket.create_connection(('127.0.0.1', HTTP_PORT), 0.4)
        s.close()
        print(f'Serial Monitor already running — opening {url}')
        _open_url(url)
        return
    except OSError:
        pass
    HTTPServer.allow_reuse_address = True
    srv = HTTPServer(('0.0.0.0', HTTP_PORT), H)
    print(f'Serial Monitor on {url}  (history {HISTORY} lines)')
    threading.Timer(0.6, lambda: _open_url(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        _close_serial(quiet=True)


if __name__ == '__main__':
    main()
