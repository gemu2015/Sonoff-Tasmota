#!/usr/bin/env python3
# =====================================================================
# native2dual_app.py — local web app, same infrastructure as
# tc2plugin_app.py: left pane = native Tasmota driver (.ino/.cpp),
# [Translate ▶] runs scaffold.py, right pane = generated dual-format
# BinPlugin C++ (editable). [Compile ⚙] forks build_plugin.py for the
# real .bin. [Save ▼]/[Restore ↩] like tc2plugin.
#
# Source picker is fed by triage.py — each driver is shown with its
# class ([CHEAP]/[SERIAL_SHIM]/[VIABLE_BIG]/[NEEDS_PORT]) so you pick
# the cheap wins first. Stdlib only; Ctrl-C to stop.
#
# Nothing is silently dropped: scaffold.py emits honest NEEDS-JMPTBL /
# NEEDS-ABI / CAN'T-SCAFFOLD markers a human finishes.
# =====================================================================
import sys, os, re, json, html, shutil, subprocess, tempfile
import webbrowser, threading, http.server, socketserver
from urllib.parse import urlparse, parse_qs

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO  = os.path.realpath(os.path.join(_HERE, '..', '..', '..', '..'))
SRC_DIRS = [                                   # native driver roots
    os.path.join(REPO, 'tasmota', 'tasmota_xsns_sensor'),
    os.path.join(REPO, 'tasmota', 'tasmota_xdrv_driver'),
]
OVERRIDE  = os.path.join(REPO, 'tasmota', 'user_config_override.h')
SCAFFOLD  = os.path.join(_HERE, 'scaffold.py')
LISTEN_PORT = 8772                              # tc2plugin uses 8771

sys.path.insert(0, _HERE)
try:
    import triage as _triage                    # for class labels
except Exception:
    _triage = None

DEFAULT_REL  = 'tasmota_xsns_sensor/xsns_14_sht3x.ino'   # a CHEAP driver
def _read(rel):
    for d in SRC_DIRS:
        if os.path.basename(d) == rel.split('/')[0]:
            p = os.path.join(d, rel.split('/', 1)[1])
            if os.path.isfile(p):
                return open(p, encoding='utf-8', errors='replace').read()
    return ''
try:
    DEFAULT_SRC = _read(DEFAULT_REL) or '// pick a driver from the list'
    DEFAULT_NAME = DEFAULT_REL
except Exception:
    DEFAULT_SRC, DEFAULT_NAME = '// (no default)', 'driver'

def _modname(raw):
    m = re.sub(r'\.(ino|cpp|c|h|txt)$', '', os.path.basename(raw))
    m = re.sub(r'\W', '_', m).strip('_').lower() or 'driver'
    return ('m_' + m) if m[0].isdigit() else m

def scaffold(src, name):
    """Run scaffold.py on the (possibly hand-edited) source via a temp
    file; return generated dual-format C++ (raises on failure)."""
    with tempfile.NamedTemporaryFile('w', suffix='.ino', delete=False,
                                     encoding='utf-8') as tf:
        tf.write(src); tmp = tf.name
    try:
        r = subprocess.run([sys.executable, SCAFFOLD, tmp, name],
                           capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            raise RuntimeError((r.stderr or r.stdout or 'scaffold failed'
                                ).strip()[:4000])
        return r.stdout
    finally:
        try: os.remove(tmp)
        except OSError: pass

def list_drivers():
    out = []
    for d in SRC_DIRS:
        root = os.path.basename(d)
        try:
            files = sorted(f for f in os.listdir(d) if f.endswith('.ino'))
        except OSError:
            files = []
        for f in files:
            cls = ''
            if _triage:
                try:
                    cat, _why, _i = _triage.classify(os.path.join(d, f))
                    cls = {'VIABLE': 'CHEAP'}.get(cat, cat)
                except Exception:
                    cls = '?'
            out.append({'v': f'{root}/{f}', 't': f'[{cls or "?"}] {f}',
                        'k': cls})
    order = {'CHEAP': 0, 'SERIAL_SHIM': 1, 'VIABLE_BIG': 2,
             'NEEDS_PORT': 3}
    out.sort(key=lambda e: (order.get(e['k'], 9), e['v']))
    return out

PAGE = '''<!doctype html><html><head><meta charset="utf-8">
<title>native2dual — native driver → BinPlugin (PoC)</title><style>
 body{margin:0;font:13px/1.4 -apple-system,Menlo,monospace;background:#1e1e1e;color:#ddd}
 header{background:#252526;padding:10px 14px;border-bottom:1px solid #333;display:flex;align-items:center;gap:14px;flex-wrap:wrap}
 header b{color:#4ec9b0;font-size:15px}
 header span{color:#888;font-size:12px}
 #go{background:#0a7;color:#fff;border:0;padding:8px 18px;border-radius:4px;cursor:pointer;font-weight:bold}
 #go:active{background:#085}
 .wrap{display:flex;height:calc(100vh - 49px)}
 .col{flex:1;display:flex;flex-direction:column;border-right:1px solid #333}
 .col:last-child{border-right:0}
 .lbl{background:#2d2d2d;color:#9cdcfe;padding:6px 12px;font-size:12px;border-bottom:1px solid #333}
 .ed{flex:1;position:relative;overflow:hidden}
 .ed pre,.ed textarea{margin:0;padding:12px;border:0;box-sizing:border-box;
   position:absolute;inset:0;overflow:auto;tab-size:4;white-space:pre;
   font:12px/1.45 Menlo,Consolas,monospace}
 .ed pre{background:#1e1e1e;color:#dcdcaa;pointer-events:none;z-index:0}
 .ed textarea{background:transparent;color:transparent;caret-color:#fff;
   resize:none;outline:none;z-index:1}
 #outwrap pre{background:#181818}
 .tk-c{color:#6a9955}.tk-s{color:#ce9178}.tk-n{color:#b5cea8}
 .tk-k{color:#569cd6}.tk-t{color:#4ec9b0}.tk-p{color:#c586c0}
</style></head><body>
<header><b>native2dual</b><span>native xsns_/xdrv_&nbsp;→&nbsp;BinPlugin C++ · PoC</span>
<label for="fp" style="background:#37373d;padding:7px 12px;border-radius:4px;cursor:pointer">📂 Open…</label>
<input id="fp" type="file" accept=".ino,.cpp,.c,.h,.txt" style="display:none">
<select id="ex" style="background:#37373d;color:#ddd;border:0;padding:7px;border-radius:4px;cursor:pointer;max-width:340px">
 <option value="">— drivers (triaged) —</option></select>
<button id="go">Translate ▶</button>
<button id="cc" style="background:#36c;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Write slot xsns_198_<name>_n2d.cpp, enable USE_<NAME>_N2D_MOD, fork build_plugin.py">Compile ⚙</button>
<button id="sv" style="background:#393;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Download the right pane verbatim as xsns_198_<name>_n2d.cpp">Save ▼</button>
<button id="rs" disabled style="background:#777;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Compile replaces the pane with the build log; toggle back to your edited source">Restore ↩</button>
<button id="quit" style="background:#a33;color:#fff;border:0;padding:8px 14px;border-radius:4px;cursor:pointer;margin-left:auto">Exit ✕</button>
<span id="st"></span></header>
<div class="wrap">
 <div class="col"><div class="lbl">Native Tasmota driver (.ino/.cpp) — faithful, no feature drop</div>
   <div class="ed"><pre id="inhl" aria-hidden="true"></pre>
   <textarea id="in" spellcheck="false">__SRC__</textarea></div></div>
 <div class="col" id="outwrap"><div class="lbl">Generated dual-format BinPlugin C++ (editable; honest NEEDS-* flags)</div>
   <div class="ed"><pre id="outhl" aria-hidden="true"></pre>
   <textarea id="out" spellcheck="false" title="Editable: finish the flagged lines then Compile. Translate ▶ regenerates and discards edits."></textarea></div></div>
</div>
<script>
const go=document.getElementById('go'),inp=document.getElementById('in'),
      out=document.getElementById('out'),st=document.getElementById('st'),
      inhl=document.getElementById('inhl'),outhl=document.getElementById('outhl');
let curName='__NAME__';
const KW=new Set(('int float char void bool if else while for return '
 +'break continue struct typedef const static unsigned signed short long '
 +'double sizeof switch case default goto enum union volatile').split(' '));
const TY=new Set(('int32_t uint32_t int16_t uint16_t int8_t uint8_t int64_t '
 +'uint64_t size_t MODULE_MEMORY bool').split(' '));
const MAC=new Set(('PSTR SETREGS SETMEMREGS ALLOCMEM RETMEM STGLOB '
 +'MODULE_PART MODULE_END MODULE_DESCRIPTOR PUSH_OPTIONS PULL_OPTIONS '
 +'BUILD_AS_PLUGIN jI2cWrite8Bus jI2cWrite0 jI2cReadBuffer0').split(' '));
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')
 .replace(/>/g,'&gt;');}
function hl(t){
 const re=/(\\/\\/[^\\n]*|\\/\\*[\\s\\S]*?\\*\\/)|("(?:\\\\.|[^"\\\\])*"|'(?:\\\\.|[^'\\\\])*')|(^[ \\t]*#[^\\n]*)|(\\b\\d[\\w.]*\\b)|([A-Za-z_]\\w*)|([^])/gm;
 let o='',m;
 while((m=re.exec(t))){
  if(m[1])o+='<span class="tk-c">'+esc(m[1])+'</span>';
  else if(m[2])o+='<span class="tk-s">'+esc(m[2])+'</span>';
  else if(m[3])o+='<span class="tk-p">'+esc(m[3])+'</span>';
  else if(m[4])o+='<span class="tk-n">'+esc(m[4])+'</span>';
  else if(m[5]){const w=m[5];o+=KW.has(w)?'<span class="tk-k">'+w+'</span>'
    :(TY.has(w)||MAC.has(w))?'<span class="tk-t">'+w+'</span>':esc(w);}
  else o+=esc(m[6]);
 }
 return o;
}
function paint(ta,pre){
 const v=ta.value;
 if(v.length>40000){pre.textContent=v;}
 else{pre.innerHTML=hl(v)+'\\n';}
 pre.scrollTop=ta.scrollTop;pre.scrollLeft=ta.scrollLeft;
}
const pIn=()=>paint(inp,inhl),pOut=()=>paint(out,outhl);
inp.addEventListener('input',pIn);
inp.addEventListener('scroll',()=>{inhl.scrollTop=inp.scrollTop;
 inhl.scrollLeft=inp.scrollLeft;});
out.addEventListener('input',pOut);
out.addEventListener('scroll',()=>{outhl.scrollTop=out.scrollTop;
 outhl.scrollLeft=out.scrollLeft;});
pIn();
let savedSrc=null,savedLog=null,showingLog=false;
const cc=document.getElementById('cc'),rs=document.getElementById('rs');
function setRestore(on){rs.disabled=!on;
  rs.style.background=on?'#a60':'#777';
  rs.textContent=on&&showingLog?'Restore ↩':(on?'Show log ↪':'Restore ↩');}
async function tr(){
  st.textContent='scaffolding…';
  try{
    const r=await fetch('/translate?name='+encodeURIComponent(curName),
                        {method:'POST',body:inp.value});
    const j=await r.json();
    out.value=j.ok?j.code:('// SCAFFOLD ERROR\\n// '+j.error);
    st.textContent=j.ok?('ok · '+j.lines+' lines · '+curName):'error';
  }catch(e){out.value='// '+e;st.textContent='error';}
  savedSrc=null;savedLog=null;showingLog=false;setRestore(false);
  pOut();
}
go.onclick=tr;
rs.onclick=()=>{
  if(savedSrc==null)return;
  if(showingLog){savedLog=out.value;out.value=savedSrc;showingLog=false;
    st.textContent='source restored — edit & Compile again, or Show log';}
  else{savedSrc=out.value;out.value=savedLog!=null?savedLog:out.value;
    showingLog=true;st.textContent='showing build log';}
  pOut();setRestore(true);};
cc.onclick=async()=>{
  if(!out.value.trim()||out.value.startsWith('// SCAFFOLD ERROR')){
    st.textContent='translate first';return;}
  const code=out.value;
  savedSrc=code;savedLog=null;showingLog=true;setRestore(false);
  cc.disabled=true;st.textContent='compiling (forking build_plugin.py)…';
  out.value='# Compile: USE_<NAME>_N2D_MOD → build_plugin.py …\\n';
  pOut();
  try{
    const r=await fetch('/compile?name='+encodeURIComponent(curName),
                        {method:'POST',body:code});
    const rd=r.body.getReader(),dec=new TextDecoder();
    for(;;){const{done,value}=await rd.read();if(done)break;
      out.value+=dec.decode(value,{stream:true});
      out.scrollTop=out.scrollHeight;pOut();}
    st.textContent='compile finished (see log)';
  }catch(e){out.value+='\\n# stream error: '+e;st.textContent='error';}
  finally{cc.disabled=false;savedLog=out.value;showingLog=true;
    setRestore(true);
    st.textContent+=' · Restore ↩ to get your edited source back';}
};
const sv=document.getElementById('sv');
sv.onclick=()=>{
  if(!out.value.trim()||out.value.startsWith('// SCAFFOLD ERROR')
     ||out.value.startsWith('# Compile')){
    st.textContent='nothing to save — translate first';return;}
  const mod=curName.replace(/.*\\//,'').replace(/\\.[^.]*$/,'')
            .replace(/\\W/g,'_').replace(/^_+|_+$/g,'').toLowerCase()
            ||'driver';
  const fn='xsns_198_'+(/^\\d/.test(mod)?'m_'+mod:mod)+'_n2d.cpp';
  const b=new Blob([out.value],{type:'text/x-c++src'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(b);a.download=fn;
  document.body.appendChild(a);a.click();a.remove();
  setTimeout(()=>URL.revokeObjectURL(a.href),1000);
  st.textContent='saved '+fn+' ('+out.value.length+' bytes)';
};
const fp=document.getElementById('fp'),ex=document.getElementById('ex');
fp.onchange=e=>{const f=e.target.files[0];if(!f)return;
  const rd=new FileReader();rd.onload=()=>{inp.value=rd.result;curName=f.name;
    pIn();st.textContent='loaded '+f.name;tr();};
  rd.readAsText(f);fp.value='';};
ex.onchange=async()=>{if(!ex.value)return;
  const r=await fetch('/file?path='+encodeURIComponent(ex.value));
  const j=await r.json();
  if(j.ok){inp.value=j.text;curName=ex.value;pIn();
    st.textContent='loaded '+ex.value;tr();}
  else{st.textContent='err: '+j.error;}};
(async()=>{try{const r=await fetch('/examples');const j=await r.json();
  for(const e of j.files){const o=document.createElement('option');
    o.value=e.v;o.textContent=e.t;ex.appendChild(o);}}catch(e){}})();
document.getElementById('quit').onclick=async()=>{
  st.textContent='shutting down…';
  try{await fetch('/quit',{method:'POST'});}catch(e){}
  document.body.innerHTML='<div style="padding:40px;font:16px -apple-system">'
    +'native2dual server stopped. You can close this tab.</div>';
  setTimeout(()=>{try{window.close();}catch(e){}},300);
};
window.onload=tr;
</script></body></html>'''


class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def _json(self, payload):
        b = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def _resolve(self, rel):
        rel = rel.lstrip('/')
        parts = rel.split('/', 1)
        if len(parts) != 2:
            return None
        for d in SRC_DIRS:
            if os.path.basename(d) == parts[0]:
                full = os.path.realpath(os.path.join(d, parts[1]))
                if full.startswith(d + os.sep) and os.path.isfile(full):
                    return full
        return None

    # ---- Compile: write slot, enable gate, fork build_plugin.py ----
    def do_compile(self, cpp_src, mod):
        NAME = mod.upper()
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain; charset=utf-8')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        self.end_headers()

        def w(s):
            try:
                self.wfile.write(s.encode('utf-8', 'replace'))
                self.wfile.flush()
            except Exception:
                pass

        use  = f'USE_{NAME}_N2D_MOD'
        bak  = OVERRIDE + '.n2d.bak'
        slot = os.path.join(REPO, 'tasmota', 'Plugins',
                            f'xsns_198_{mod}_n2d.cpp')
        bp   = os.path.join(REPO, 'tasmota', 'Plugins', 'build_plugin.py')
        try:
            w(f"# native2dual compile — '{mod}'  gate {use}\n")
            with open(slot, 'w') as f:
                f.write(cpp_src)
            w(f"# wrote {os.path.relpath(slot, REPO)}\n")
            shutil.copy2(OVERRIDE, bak)
            txt = open(OVERRIDE, encoding='utf-8', errors='replace').read()
            if use not in txt:
                end = '// >>> PLUGIN_DEFINES_END'
                txt = txt.replace(end, f'//#define {use}\n{end}', 1)
                open(OVERRIDE, 'w', encoding='utf-8').write(txt)
                w(f"# added //#define {use} to PLUGIN_DEFINES section\n")
            cmd = [sys.executable, bp, '--plugin', use, '--cpu', 'esp32']
            w(f"\n# $ build_plugin.py --plugin {use} --cpu esp32\n"
              f"#   (forked — live log, takes minutes)\n" + "-" * 64 + "\n")
            p = subprocess.Popen(cmd, cwd=REPO, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, bufsize=1,
                                 text=True)
            for line in p.stdout:
                w(line)
            rc = p.wait()
            w("-" * 64 + f"\n# build_plugin.py exit code: {rc}\n")
            outdir = os.path.join(REPO, 'build_output', 'firmware')
            found = []
            for root, _, files in os.walk(outdir):
                for b in files:
                    if b.endswith('.bin') and (mod[:5].lower() in b.lower()
                                               or NAME in b):
                        fp = os.path.join(root, b)
                        found.append((os.path.relpath(fp, REPO),
                                      os.path.getsize(fp)))
            if rc == 0 and found:
                for rel, sz in sorted(set(found)):
                    w(f"# PLUGIN BIN: {rel}  ({sz} bytes)\n")
            elif rc == 0:
                w("# build OK but no matching .bin found\n")
            else:
                w("# build FAILED — see errors above (NEEDS-* flags in "
                  "the generated source mark the human-finish items)\n")
        except Exception as e:
            w(f"\n# native2dual compile error: {type(e).__name__}: {e}\n")
        finally:
            try:
                if os.path.exists(bak):
                    shutil.move(bak, OVERRIDE)
            except Exception:
                pass
            try:
                os.remove(slot)
            except OSError:
                pass
            w("# cleaned up: override.h restored, temp .cpp removed\n")

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == '/examples':
            try:
                return self._json({'files': list_drivers()})
            except Exception as e:
                return self._json({'files': [], 'error': str(e)})
        if u.path == '/file':
            q = parse_qs(u.query).get('path', [''])[0]
            full = self._resolve(q)
            if not full:
                return self._json({'ok': False, 'error': 'not found'})
            try:
                with open(full, encoding='utf-8', errors='replace') as fh:
                    return self._json({'ok': True, 'text': fh.read()})
            except OSError as e:
                return self._json({'ok': False, 'error': str(e)})
        body = (PAGE.replace('__SRC__', html.escape(DEFAULT_SRC))
                    .replace('__NAME__', DEFAULT_NAME)).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if urlparse(self.path).path == '/quit':
            self._json({'ok': True})
            threading.Timer(0.3, lambda: os._exit(0)).start()
            return
        n = int(self.headers.get('Content-Length', 0))
        src = self.rfile.read(n).decode('utf-8', 'replace')
        raw = parse_qs(urlparse(self.path).query).get('name', [''])[0]
        mod = _modname(raw)
        if urlparse(self.path).path == '/compile':
            return self.do_compile(src, mod)
        try:
            code = scaffold(src, mod)
            payload = {'ok': True, 'code': code,
                       'lines': code.count('\n') + 1}
        except Exception as e:
            payload = {'ok': False, 'error': f'{type(e).__name__}: {e}'}
        b = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(b)))
        self.end_headers()
        self.wfile.write(b)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--cli':
        print(scaffold(sys.stdin.read(),
                        _modname(sys.argv[2] if len(sys.argv) > 2
                                 else 'driver')))
        return
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(('127.0.0.1', LISTEN_PORT), H) as s:
        url = f'http://127.0.0.1:{LISTEN_PORT}/'
        print(f'native2dual running → {url}  (Ctrl-C to stop)')
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
        try:
            s.serve_forever()
        except KeyboardInterrupt:
            print('\nstopped.')


if __name__ == '__main__':
    main()
