#!/usr/bin/env python3
# =====================================================================
# tc2plugin — Proof of Concept: TinyC -> Tasmota dual-format plugin C++
#
# A local web app (stdlib only, no pip installs): left pane = TinyC
# source, [Translate] button, right pane = generated dual-format
# plugin C++ (the xsns_*_dual.cpp shape used by xdrv_123_plugins).
#
# SCOPE (this is a PoC, not a finished compiler):
#   - declarations: int/float/char[]/arrays, optional `persist`, init
#   - functions:    void/int/float name(params) { ... }
#   - statements:   block, if/else, while, for, return, expr, local decl
#   - expressions:  int/float/string/char literals, ident, index, call,
#                   unary, binary, assignment (= += -= *= /=), parens
#   - callbacks:    main()->INIT, EverySecond()->EVERY_SECOND,
#                   Command()->COMMAND, WebCall()/WebUI()->WEB_SENSOR
#   - syscalls:     curated map; unknown calls emitted verbatim with a
#                   /* TODO: bind via JMPTBL */ marker
#
# Anything the subset parser can't handle is emitted as a commented
# `// TC-PASSTHROUGH:` line — nothing is silently dropped (same
# anti-silent-loss philosophy as the TinyC VM hardening work).
# =====================================================================
import sys, os, re, json, html, shutil, subprocess
import webbrowser, threading, http.server, socketserver
from urllib.parse import urlparse, parse_qs
from syscall_abi_map import lookup as abi_lookup

# app lives at REPO/tasmota/tinyc/utils/tc2plugin/
_HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.realpath(os.path.join(_HERE, '..', '..', '..', '..'))
EXAMPLES_DIR = os.path.join(REPO, 'tasmota', 'tinyc', 'examples')

# --- plugin-compile wiring (fixed reserved slot, always cleaned up) ---
OVERRIDE  = os.path.join(REPO, 'tasmota', 'user_config_override.h')
PIO       = os.path.expanduser('~/.platformio/penv/bin/pio')
PLUGIN_ENV = 'tasmota32-plugin'
SLOT_CPP  = os.path.join(REPO, 'tasmota', 'Plugins',
                         'xsns_198_tc2plugin_dual.cpp')
SLOT_INO  = os.path.join(REPO, 'tasmota', 'tasmota_xsns_sensor',
                         'xsns_198_tc2plugin_dual.ino')
BUILD_DIR = os.path.join(REPO, '.pio', 'build', PLUGIN_ENV)

LISTEN_PORT = 8771

# Crash-hunt instrumentation: emit AddLog("TCDBG>…") at the entry of
# every generated function / helper / dispatch case so a serial
# terminal shows exactly how far init got before a hard fault.
# When True the trace lines are still EMITTED but wrapped in
# `#ifdef TC2PLUGIN_DBG` and that macro ships commented-out — so the
# default build is silent and you re-enable by uncommenting one line
# in the generated .cpp (no re-translate). Set False to omit the
# trace scaffolding entirely.
DBG_TRACE = True

# --------------------------------------------------------------------
# Default sample: a self-contained SML energy-meter emulator in TinyC
# --------------------------------------------------------------------
_FALLBACK_TC = r'''// SML energy-meter emulator (TinyC) — tc2plugin demo
//
// Emulates a smart meter: a power value wanders, energy integrates
// from it every second, and a console command reports the live JSON.

persist float energy_in;     // kWh imported (persisted across reboots)
persist float energy_out;    // kWh exported
persist int   boot_count;

float power_w;               // current (signed) power, W
int   tick;

void EverySecond() {
    // crude wandering load: -800 .. +1200 W
    tick = tick + 1;
    power_w = 200.0 + 500.0 * sinApprox(tick);
    // integrate Ws -> kWh  (1 s tick)
    if (power_w >= 0.0) {
        energy_in = energy_in + power_w / 3600000.0;
    } else {
        energy_out = energy_out - power_w / 3600000.0;
    }
}

void Command() {
    char buf[256];
    sprintf(buf, "{\"SMLemu\":{\"P\":%d,\"Ein\":%s,\"Eout\":%s,\"boot\":%d}}",
            (int)power_w, ftoa(energy_in), ftoa(energy_out), boot_count);
    responseCmnd(buf);
}

float sinApprox(int t) {
    // cheap triangle wave in -1..1, period 60
    int p;
    p = t % 60;
    if (p < 30) { return (p - 15) / 15.0; }
    return (45 - p) / 15.0;
}

int main() {
    boot_count = boot_count + 1;
    tick = 0;
    addCommand("SMLEMU");
    addLog("sml emulator started");
    return 0;
}
'''

# Default test file = sht31_th (the T+H-only live-test variant: it
# carries the init-contract fix and has NO log/exp transcendentals,
# so it's the actual end-to-end plugin-compile / .39 target. The
# canonical sht31.tc is deliberately left untouched — it still has
# the jt-routed log()/exp() that trip the open Bug 3). Falls back to
# the embedded SML-emu sample if the example file is missing.
DEFAULT_NAME = 'sht31_th.tc'
try:
    DEFAULT_TC = open(os.path.join(EXAMPLES_DIR, DEFAULT_NAME),
                      encoding='utf-8', errors='replace').read()
except OSError:
    DEFAULT_TC, DEFAULT_NAME = _FALLBACK_TC, 'sml_emu'

# --------------------------------------------------------------------
# Lexer
# --------------------------------------------------------------------
KEYWORDS = {'int','float','char','void','if','else','while','for',
            'return','persist','break','continue'}
TOK = re.compile(r'''
    (?P<ws>\s+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<flt>\d+\.\d+)
  | (?P<int>0x[0-9A-Fa-f]+|\d+)
  | (?P<str>"(?:\\.|[^"\\])*")
  | (?P<chr>'(?:\\x[0-9A-Fa-f]+|\\[0-7]{1,3}|\\.|[^'\\])')
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><<=|>>=|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=|\+\+|--|<<|>>|[-+*/%=<>!&|^~?:.,;(){}\[\]])
''', re.VERBOSE | re.DOTALL)


class Tok:
    __slots__ = ('k', 'v', 'pos')
    def __init__(self, k, v, pos): self.k, self.v, self.pos = k, v, pos
    def __repr__(self): return f'{self.k}:{self.v!r}'


def lex(src):
    out, i = [], 0
    while i < len(src):
        m = TOK.match(src, i)
        if not m:
            raise SyntaxError(f'lex error at {i}: {src[i:i+20]!r}')
        i = m.end()
        k = m.lastgroup
        if k in ('ws', 'lc', 'bc'):
            continue
        v = m.group()
        if k == 'id' and v in KEYWORDS:
            k = 'kw'
        out.append(Tok(k, v, m.start()))
    out.append(Tok('eof', '', len(src)))
    return out


# --------------------------------------------------------------------
# Parser  (subset recursive descent). AST nodes are dicts {'n': kind, ...}
# On any construct it doesn't understand it raises ParseSkip so the
# emitter can pass the original source line(s) through as a comment.
# --------------------------------------------------------------------
class ParseSkip(Exception):
    pass


class Parser:
    def __init__(self, toks, src):
        self.t, self.i, self.src = toks, 0, src

    def peek(self, o=0): return self.t[self.i + o]
    def nx(self):
        tk = self.t[self.i]; self.i += 1; return tk
    def eat(self, v):
        tk = self.t[self.i]
        if tk.v != v:
            raise ParseSkip(f'expected {v!r} got {tk.v!r}')
        self.i += 1; return tk
    def isv(self, v): return self.t[self.i].v == v

    TYPES = {'int', 'float', 'char', 'void'}

    def parse(self):
        decls = []
        while self.peek().k != 'eof':
            start = self.i
            try:
                decls.append(self.top())
            except ParseSkip:
                # recover: capture raw source until next top-level ; or }
                self.i = start
                raw = self.skip_top_raw()
                decls.append({'n': 'raw', 'src': raw})
        return decls

    def skip_top_raw(self):
        start_pos = self.peek().pos
        depth = 0
        while self.peek().k != 'eof':
            tk = self.nx()
            if tk.v == '{':
                depth += 1
            elif tk.v == '}':
                depth -= 1
                if depth == 0:
                    break
            elif tk.v == ';' and depth == 0:
                break
        end_pos = self.peek().pos
        return self.src[start_pos:end_pos].strip()

    def top(self):
        persist = False
        if self.isv('persist'):
            self.eat('persist'); persist = True
        if self.peek().v not in self.TYPES:
            raise ParseSkip('not a decl/func')
        ty = self.nx().v
        name = self.nx()
        if name.k != 'id':
            raise ParseSkip('bad name')
        if self.isv('('):                       # function
            params = self.params()
            body = self.block()
            return {'n': 'func', 'ret': ty, 'name': name.v,
                    'params': params, 'body': body}
        # global var (maybe array, maybe init), allow comma list
        gv = self.var_tail(ty, name.v, persist)
        return gv

    def params(self):
        self.eat('(')
        ps = []
        if not self.isv(')'):
            while True:
                ty = self.nx().v
                nm = self.nx().v
                arr = False
                if self.isv('['):
                    self.eat('[');
                    while not self.isv(']'): self.nx()
                    self.eat(']'); arr = True
                ps.append({'ty': ty, 'name': nm, 'arr': arr})
                if self.isv(','): self.eat(','); continue
                break
        self.eat(')')
        return ps

    def var_tail(self, ty, name, persist):
        # name already consumed; handle [size], = init
        decls = []
        while True:
            arr = None
            if self.isv('['):
                self.eat('[')
                sz = None
                if not self.isv(']'):
                    sz = self.expr()
                self.eat(']')
                arr = sz if sz is not None else {'n': 'int', 'v': 0}
            init = None
            if self.isv('='):
                self.eat('=')
                init = self.expr()
            decls.append({'n': 'var', 'ty': ty, 'name': name,
                          'arr': arr, 'init': init, 'persist': persist})
            if self.isv(','):
                self.eat(',')
                name = self.nx().v
                continue
            break
        self.eat(';')
        return decls[0] if len(decls) == 1 else {'n': 'multi', 'list': decls}

    def block(self):
        self.eat('{')
        stmts = []
        while not self.isv('}'):
            if self.peek().k == 'eof':
                raise ParseSkip('unterminated block')
            stmts.append(self.stmt())
        self.eat('}')
        return {'n': 'block', 'body': stmts}

    def stmt(self):
        start = self.i
        try:
            return self._stmt()
        except ParseSkip:
            self.i = start
            sp = self.peek().pos
            depth = 0
            while self.peek().k != 'eof':
                tk = self.nx()
                if tk.v == '{': depth += 1
                elif tk.v == '}':
                    if depth == 0: self.i -= 1; break
                    depth -= 1
                elif tk.v == ';' and depth == 0:
                    break
            return {'n': 'raw', 'src': self.src[sp:self.peek().pos].strip()}

    def _stmt(self):
        v = self.peek().v
        if v == '{': return self.block()
        if v == 'if':
            self.eat('if'); self.eat('('); c = self.expr(); self.eat(')')
            th = self.stmt(); el = None
            if self.isv('else'):
                self.eat('else'); el = self.stmt()
            return {'n': 'if', 'c': c, 'th': th, 'el': el}
        if v == 'while':
            self.eat('while'); self.eat('('); c = self.expr(); self.eat(')')
            return {'n': 'while', 'c': c, 'body': self.stmt()}
        if v == 'for':
            self.eat('for'); self.eat('(')
            init = None if self.isv(';') else self.simple()
            self.eat(';')
            cond = None if self.isv(';') else self.expr()
            self.eat(';')
            post = None if self.isv(')') else self.expr_or_assign()
            self.eat(')')
            return {'n': 'for', 'init': init, 'cond': cond,
                    'post': post, 'body': self.stmt()}
        if v == 'return':
            self.eat('return')
            e = None if self.isv(';') else self.expr()
            self.eat(';')
            return {'n': 'return', 'e': e}
        if v in ('break', 'continue'):
            self.eat(v); self.eat(';')
            return {'n': v}
        if v in self.TYPES:                    # local declaration
            ty = self.nx().v
            nm = self.nx().v
            return self.var_tail(ty, nm, False)
        s = self.simple(); self.eat(';'); return s

    def simple(self):
        return {'n': 'exprstmt', 'e': self.expr_or_assign()}

    ASSIGN_OPS = ('=', '+=', '-=', '*=', '/=', '%=',
                  '&=', '|=', '^=', '<<=', '>>=')

    def expr_or_assign(self):
        lhs = self.expr()
        if self.peek().v in self.ASSIGN_OPS:
            op = self.nx().v
            rhs = self.expr_or_assign()
            return {'n': 'assign', 'op': op, 'lhs': lhs, 'rhs': rhs}
        return lhs

    # expression precedence ladder (C precedence, high -> low at bottom)
    def expr(self): return self.p_or()
    def _bin(self, sub, ops):
        l = sub()
        while self.peek().v in ops:
            op = self.nx().v
            l = {'n': 'bin', 'op': op, 'l': l, 'r': sub()}
        return l
    def p_or(self):    return self._bin(self.p_and,  ('||',))
    def p_and(self):   return self._bin(self.p_bor,  ('&&',))
    def p_bor(self):   return self._bin(self.p_bxor, ('|',))
    def p_bxor(self):  return self._bin(self.p_band, ('^',))
    def p_band(self):  return self._bin(self.p_eq,   ('&',))
    def p_eq(self):    return self._bin(self.p_rel,  ('==', '!='))
    def p_rel(self):   return self._bin(self.p_shift,
                                        ('<', '>', '<=', '>='))
    def p_shift(self): return self._bin(self.p_add,  ('<<', '>>'))
    def p_add(self):   return self._bin(self.p_mul,  ('+', '-'))
    def p_mul(self):   return self._bin(self.p_un,   ('*', '/', '%'))

    def p_un(self):
        if self.peek().v in ('-', '!', '+', '~'):
            op = self.nx().v
            return {'n': 'un', 'op': op, 'e': self.p_un()}
        if self.peek().v in ('++', '--'):
            op = self.nx().v
            return {'n': 'pre', 'op': op, 'e': self.p_un()}
        if self.peek().v == '(' and self.peek(1).v in self.TYPES \
                and self.peek(2).v == ')':
            self.eat('('); ty = self.nx().v; self.eat(')')
            return {'n': 'cast', 'ty': ty, 'e': self.p_un()}
        return self.p_post()

    def p_post(self):
        e = self.p_prim()
        while True:
            if self.isv('['):
                self.eat('['); idx = self.expr(); self.eat(']')
                e = {'n': 'index', 'a': e, 'i': idx}
            elif self.isv('('):
                self.eat('(')
                args = []
                if not self.isv(')'):
                    while True:
                        args.append(self.expr())
                        if self.isv(','): self.eat(','); continue
                        break
                self.eat(')')
                e = {'n': 'call', 'f': e, 'args': args}
            elif self.peek().v in ('++', '--'):
                op = self.nx().v
                e = {'n': 'post', 'op': op, 'e': e}
            else:
                break
        return e

    def p_prim(self):
        tk = self.peek()
        if tk.v == '(':
            self.eat('('); e = self.expr(); self.eat(')'); return e
        if tk.k == 'int':
            self.nx(); return {'n': 'int', 'v': tk.v}
        if tk.k == 'flt':
            self.nx(); return {'n': 'flt', 'v': tk.v}
        if tk.k == 'str':
            self.nx(); return {'n': 'str', 'v': tk.v}
        if tk.k == 'chr':
            self.nx(); return {'n': 'chr', 'v': tk.v}
        if tk.k == 'id':
            self.nx(); return {'n': 'id', 'v': tk.v}
        raise ParseSkip(f'unexpected {tk.v!r}')


# --------------------------------------------------------------------
# Emitter: AST -> dual-format plugin C++
# --------------------------------------------------------------------
# TinyC syscall -> plugin-side rendering. None => emit verbatim + TODO.
SYSCALL_MAP = {
    'addLog':      ('AddLog(LOG_LEVEL_INFO, PSTR("%s")', 'log'),
    'responseCmnd': ('ResponseCmndChar', 'plain'),
    'addCommand':  (None, 'noop'),     # registration handled by descriptor
    'sprintf':     ('snprintf_wrap', 'sprintf'),
    'millis':      ('millis', 'plain'),
}
# builtins we map to a small compat shim emitted in the header
SHIM_BUILTINS = {'ftoa', 'sinApprox'}  # sinApprox is user-defined; ftoa shimmed


class Emitter:
    def __init__(self, decls, modname, defines=None, passthru=None):
        self.decls = decls
        self.mod = modname
        self.defines = defines or []
        self.passthru = passthru or []
        self.globals = []     # (cdecl, name)
        self.funcs = {}       # name -> node
        self.out = []
        self.todo = []                 # genuinely unknown syscalls
        self.resolved = set()          # mapped to fixed ABI
        self.vm_internal = {}          # VM constructs (no jt[] equiv)
        self.tenv = {}                 # name -> type code (per function)
        self.fn_abi = False            # cur fn emitted a jt[]-routed call
        self.i2c_helpers = set()       # generated tc_i2c_* shims needed
        self.need_lstr = False         # LGetString -> emit D_* PROGMEM tbl
        self.fpool = []                # ordered unique float-const texts
        self.fidx = {}                 # normalised text -> FP_CONST index
        self.in_decl_size = False      # suppress ICONST in array bounds

    def emit(self):
        for d in self.decls:
            self._collect(d)
        return self._render()

    def _collect(self, d):
        if d['n'] == 'multi':
            for x in d['list']:
                self._collect(x)
        elif d['n'] == 'var':
            self.globals.append(d)
        elif d['n'] == 'func':
            self.funcs[d['name']] = d
        elif d['n'] == 'raw':
            self.globals.append(d)

    # ---- type + expression rendering ----
    CTYPE = {'int': 'int32_t', 'float': 'float', 'char': 'char', 'void': 'void'}

    def cty(self, ty): return self.CTYPE.get(ty, ty)

    def g_decl(self, d):
        ct = self.cty(d['ty'])
        if d['arr'] is not None:
            self.in_decl_size = True            # array bound: no ICONST
            sz = self.ex(d['arr'])
            self.in_decl_size = False
            t = 'char' if d['ty'] == 'char' else ct
            return f'{t} {d["name"]}[{sz}];'
        return f'{ct} {d["name"]};'

    # ---- type inference (drives float-op lowering) ----
    # 'f'=float  'i'=int/char  's'=string/ptr  '?'=unknown(treat as int)
    def _tcode(self, ty):
        return 'f' if ty == 'float' else ('s' if ty == 'char' else 'i')

    def build_tenv(self, f):
        env = {}
        for g in self.globals:                 # globals
            if g['n'] == 'var':
                env[g['name']] = self._tcode(g['ty'])
        for p in f['params']:                  # params
            env[p['name']] = self._tcode(p['ty'])
        def walk(nd):                          # local declarations
            if isinstance(nd, dict):
                if nd.get('n') == 'var':
                    env[nd['name']] = self._tcode(nd['ty'])
                for v in nd.values():
                    walk(v)
            elif isinstance(nd, list):
                for x in nd:
                    walk(x)
        walk(f['body'])
        self.tenv = env

    def typeof(self, e):
        n = e['n']
        if n == 'flt':  return 'f'
        if n == 'int':  return 'i'
        if n in ('str', 'chr'): return 's'
        if n == 'id':   return self.tenv.get(e['v'], 'i')
        if n == 'paren': return self.typeof(e['e'])
        if n == 'un':   return 'i' if e['op'] == '!' else self.typeof(e['e'])
        if n in ('pre', 'post'): return self.typeof(e['e'])
        if n == 'cast': return self._tcode(e['ty'])
        if n == 'index': return self.tenv.get(
            e['a'].get('v') if e['a']['n'] == 'id' else '', 'i')
        if n == 'assign': return self.typeof(e['lhs'])
        if n == 'bin':
            if e['op'] in ('==', '!=', '<', '>', '<=', '>=', '&&', '||'):
                return 'i'
            if e['op'] in ('+', '-', '*', '/'):
                return 'f' if 'f' in (self.typeof(e['l']),
                                      self.typeof(e['r'])) else 'i'
            return 'i'                          # bitwise/% -> int
        if n == 'call':
            fn = e['f'].get('v') if e['f']['n'] == 'id' else None
            if fn == 'ftoa': return 's'
            if fn in self.funcs:
                return self._tcode(self.funcs[fn]['ret'])
            k, info = abi_lookup(fn) if fn else (None, None)
            if k == 'abi':
                return info.get('ret', 'i')
            return 'i'
        return 'i'

    # render an expr coerced into float context (no FP intrinsic):
    #   float-typed     -> as is
    #   int literal     -> compile-time float constant (no call)
    #   int expression  -> tofloat(...)  (jt[] in plugin)
    def fx(self, e):
        if self.typeof(e) == 'f':
            return self.ex(e)
        if e['n'] == 'int':                    # int literal in float ctx
            try:                               # handle hex/oct too
                return self.fconst(repr(float(int(e['v'], 0))))
            except ValueError:
                return self.fconst(e['v'])
        return f'tofloat({self.ex(e)})'

    # ---- constant pooling (relocatable-plugin requirement) ----
    # Floats can't be Xtensa immediates and the literal pool isn't
    # relocated in plugin mode -> every float constant goes to a
    # PROGMEM array, referenced via FLTC(idx) (compat header maps it
    # native=array index / plugin=EXEC_OFFSET PROGMEM read).
    def fconst(self, text):
        t = str(text).strip().rstrip('fF')
        try:
            val = float(t)
        except ValueError:
            # never crash the whole translation on an odd literal
            return f'({text}f)/*FIXME: float const not pooled*/'
        key = repr(val)
        if key not in self.fidx:
            self.fidx[key] = len(self.fpool)
            lit = t if ('.' in t or 'e' in t.lower()) else t + '.0'
            self.fpool.append(lit + 'f')
        return f'FLTC({self.fidx[key]})'

    # Integer literals wider than a 12-bit signed immediate (|v|>2047)
    # need the literal pool too -> wrap in ICONST() (native passthrough,
    # plugin PROGMEM-routed). Shift counts / small masks stay inline.
    def iconst(self, text):
        text = str(text)
        try:
            v = int(text, 0)
        except ValueError:
            return text
        if self.in_decl_size or -2048 <= v <= 2047:
            return text
        return f'ICONST({text})'

    SF = {'+': 'fadd', '-': 'fdiff', '*': 'fmul', '/': 'fdiv'}
    SFCMP = {'<': 'ltsf2', '>': 'gtsf2', '==': 'eqsf2',
             '<=': '!gtsf2', '>=': '!ltsf2', '!=': '!eqsf2'}

    def ex(self, e):
        n = e['n']
        if n == 'int': return self.iconst(e['v'])
        if n == 'flt': return self.fconst(e['v'])
        # Every string literal -> PSTR(): in the relocatable-plugin
        # model inline string literals aren't relocated (and the _P /
        # Response_P / AddLog / snprintf_P format arg MUST be PROGMEM),
        # so a bare literal crashes. PSTR() puts it in flash.
        if n == 'str': return f'PSTR({e["v"]})'
        if n == 'chr': return e['v']     # char literal = int, not text
        if n == 'id':  return e['v']
        if n == 'paren': return '(' + self.ex(e['e']) + ')'
        if n == 'un':
            if e['op'] == '-' and self.typeof(e['e']) == 'f':
                return f'fdiff({self.fconst("0.0")}, {self.fx(e["e"])})'
            return e['op'] + self.ex(e['e'])
        if n == 'post': return self.ex(e['e']) + e['op']
        if n == 'pre':  return e['op'] + self.ex(e['e'])
        if n == 'cast':
            te = self.typeof(e['e'])
            if e['ty'] == 'float' and te != 'f':
                return self.fx(e['e'])
            if e['ty'] in ('int',) and te == 'f':
                return f'((int32_t)fixunssfsi({self.ex(e["e"])}))'
            return f'(({self.cty(e["ty"])}){self.ex(e["e"])})'
        if n == 'bin':
            op = e['op']
            isf = 'f' in (self.typeof(e['l']), self.typeof(e['r']))
            if isf and op in self.SF:
                return f'{self.SF[op]}({self.fx(e["l"])}, {self.fx(e["r"])})'
            if isf and op in self.SFCMP:
                h = self.SFCMP[op]
                neg = h.startswith('!')
                fn = h[1:] if neg else h
                inner = f'{fn}({self.fx(e["l"])}, {self.fx(e["r"])})'
                return f'(!{inner})' if neg else inner
            return f'({self.ex(e["l"])} {op} {self.ex(e["r"])})'
        if n == 'index':
            return f'{self.ex(e["a"])}[{self.ex(e["i"])}]'
        if n == 'assign':
            lt, rt = self.typeof(e['lhs']), self.typeof(e['rhs'])
            L = self.ex(e['lhs'])
            if e['op'] in ('+=', '-=', '*=', '/=') and lt == 'f':
                base = {'+=': '+', '-=': '-', '*=': '*', '/=': '/'}[e['op']]
                return (f'{L} = {self.SF[base]}({L}, '
                        f'{self.fx(e["rhs"])})')
            if e['op'] == '=' and lt == 'f' and rt != 'f':
                return f'{L} = {self.fx(e["rhs"])}'
            if e['op'] == '=' and lt != 'f' and rt == 'f':
                return f'{L} = (int32_t)fixunssfsi({self.ex(e["rhs"])})'
            return f'{L} {e["op"]} {self.ex(e["rhs"])}'
        if n == 'call':
            return self.call(e)
        return f'/*?{n}?*/'

    def call(self, e):
        fname = e['f'].get('v') if e['f']['n'] == 'id' else None
        args = [self.ex(a) for a in e['args']]
        A = ', '.join(args)
        if fname in self.funcs:                       # user function
            return f'tc_{fname}({A})'                 # defs are tc_-prefixed
        if fname is None:
            return '/*indirect call*/ 0'
        # special: addLog needs a PSTR fmt; addCommand is descriptor-side;
        # ftoa is our MODULE_MEMORY shim
        if fname == 'addLog':
            self.fn_abi = True         # AddLog -> jAddLog (jt[])
            # RULE: exactly ONE PSTR() per generated code line.
            # The str-node (ex(): n=='str') already wraps a string
            # literal in exactly one PSTR(). So when the TinyC format
            # arg is a literal, args[0] IS the PROGMEM fmt — emit it +
            # the printf varargs straight through (one PSTR, total).
            # Wrapping it again in PSTR("%s") was the .39 crash: two
            # PSTR per line + the real fmt/varargs silently dropped.
            if e['args'] and e['args'][0]['n'] == 'str':
                return f'AddLog(LOG_LEVEL_INFO, {", ".join(args)})'
            # Non-literal first arg (runtime char[]/expr): there is no
            # literal to use as the PROGMEM fmt, so we supply our own
            # single PSTR("%s") and pass the buffer as the %s value.
            return (f'AddLog(LOG_LEVEL_INFO, PSTR("%s"), '
                    f'{args[0] if args else "0"})')
        if fname == 'addCommand':
            return f'/* addCommand({A}) -> see MODULE_DESCRIPTOR */ 0'
        if fname == 'ftoa':
            return f'tc_ftoa({A})'
        # consult the fixed-ABI equivalence table
        kind, info = abi_lookup(fname)
        if kind == 'abi':
            self.resolved.add(fname)
            self.fn_abi = True         # jt[]-routed -> needs SETREGS
            note = f'  /* {info["note"]} */' if info.get('note') else ''
            if info.get('rewrite') == 'sprintf':     # buf,fmt,.. reshape
                return (f'snprintf_P({args[0]}, sizeof({args[0]}), '
                        f'{", ".join(args[1:])}){note}')
            adapt = info.get('args')                 # arity/order adapter
            call_args = adapt(args) if adapt else args
            return f'{info["emit"]}({", ".join(call_args)}){note}'
        if kind == 'helper':                         # tc_i2c_* shim
            self.i2c_helpers.add(info)               # info = 'w8'|'r8'|'rbuf'
            self.resolved.add(fname)
            return f'tc_i2c_{info}({A})'
        if kind == 'lstr':       # LGetString(index,dst) -> working-driver
            self.need_lstr = True            # GetTextIndexed pattern
            self.fn_abi = True               # jGetTextIndexed is jt[]-routed
            self.resolved.add(fname)
            if len(args) >= 2:
                return (f'GetTextIndexed({args[1]}, sizeof({args[1]}), '
                        f'{args[0]}, GSTR(tc_lstr_tbl))')
            return f'/* {fname}({A}) — need (index,dst) */ 0'
        if kind == 'manual':
            self.vm_internal[fname] = info
            return (f'/* {fname}({A}) — NO 1:1 FIXED-ABI CALL: '
                    f'{info} */ 0')
        if kind == 'vm':
            self.vm_internal[fname] = info
            return (f'/* {fname}({A}) — NO JMPTBL EQUIVALENT: '
                    f'{info} */ 0')
        # genuinely unknown — verbatim + flag for the footer list
        self.todo.append(fname)
        return (f'{fname}({A}) '
                f'/* TODO: bind {fname} via JMPTBL in xdrv_123_plugins */')

    # ---- statement rendering ----
    def st(self, s, ind):
        p = '  ' * ind
        n = s['n']
        if n == 'block':
            lines = [p + '{']
            for x in s['body']:
                lines.append(self.st(x, ind + 1))
            lines.append(p + '}')
            return '\n'.join(lines)
        if n == 'var':
            base = self.g_decl(s)
            if s.get('init') is not None:
                base = base[:-1] + f' = {self.ex(s["init"])};'
            return p + base
        if n == 'multi':
            return '\n'.join(self.st(x, ind) for x in s['list'])
        if n == 'exprstmt':
            return p + self.ex(s['e']) + ';'
        if n == 'assign':
            return p + self.ex(s) + ';'
        if n == 'if':
            r = p + f'if ({self.ex(s["c"])})\n' + self.st(s['th'], ind)
            if s['el']:
                r += '\n' + p + 'else\n' + self.st(s['el'], ind)
            return r
        if n == 'while':
            return p + f'while ({self.ex(s["c"])})\n' + self.st(s['body'], ind)
        if n == 'for':
            init = self.ex(s['init']['e']) if s['init'] else ''
            cond = self.ex(s['cond']) if s['cond'] else ''
            post = self.ex(s['post']) if s['post'] else ''
            return (p + f'for ({init}; {cond}; {post})\n'
                    + self.st(s['body'], ind))
        if n == 'return':
            if getattr(self, '_in_init', False):
                # main() == pFUNC_INIT. The returned value IS the
                # init-success signal: > 0 == sensor found / init OK,
                # <= 0 == init failed. Wire it to the plugin
                # `initialized` flag at EVERY return path (incl. early
                # returns) so the loader and our dispatcher's
                # `return tc_main()` see a truthful result. The
                # block-scoped temp keeps this safe with multiple
                # returns; the assignment stays inside tc_main's
                # ALLOCMEM-bound scope (mt valid — no gettbl()-NULL
                # fault, unlike a 2nd SETREGS in the dispatcher).
                if s['e'] is None:
                    return p + '{ initialized = 0; return 0; }'
                return (p + '{ int32_t _tc_ret = (' + self.ex(s['e'])
                        + '); initialized = (_tc_ret > 0);'
                          ' return _tc_ret; }')
            return p + ('return;' if s['e'] is None
                        else f'return {self.ex(s["e"])};')
        if n in ('break', 'continue'):
            return p + n + ';'
        if n == 'raw':
            return p + '// TC-PASSTHROUGH: ' + s['src'].replace('\n', ' ')
        return p + f'/* unhandled stmt {n} */'

    # TinyC callback -> plugin dispatch (pFUNC_* — the BUILD_AS_PLUGIN
    # path, matching xsns_*_dual.cpp). bool = returns int32 to loader.
    # Generated I2C shims — encapsulate I2C_SETWIRE(bus) + the raw
    # I2C_* sequence so they use the per-instance wire (mem->xWire /
    # _dual_wire) exactly like the hand-written dual drivers, yet are
    # callable from expression context. SETREGS gives mp/jt scope.
    # key -> (ret, name, params, [body lines])
    # I2C_SETWIRE(bus) (= plugin New_SWI2C(...)) creates/selects the
    # wire and must run ONCE per bus at the scan/probe point — NOT in
    # every primitive (repeated New_SWI2C corrupts SWI2C state). Mirrors
    # the working driver: SETWIRE in the per-bus scan loop (== our
    # i2cSetDevice/setdev), raw I2C_* afterwards on the established wire.
    I2C_HELPERS = {
        'w8': ('bool', 'tc_i2c_w8',
               '(uint8_t a, uint8_t r, uint8_t v, uint8_t bus)',
               ['(void)bus;',
                'I2C_beginTransmission(a); I2C_write(r); I2C_write(v);',
                'return I2C_endTransmission(true) == 0;']),
        'r8': ('int32_t', 'tc_i2c_r8',
               '(uint8_t a, uint8_t r, uint8_t bus)',
               ['(void)bus;',
                'I2C_beginTransmission(a); I2C_write(r); '
                'I2C_endTransmission(true);',
                'I2C_requestFrom(a, (uint8_t)1);',
                'return I2C_read();']),
        'rbuf': ('bool', 'tc_i2c_rbuf',
                 '(uint8_t a, char* buf, uint8_t n, uint8_t bus)',
                 ['(void)bus;',
                  'I2C_requestFrom(a, (uint8_t)n);',
                  'for (uint8_t i = 0; i < n; i++) '
                  'buf[i] = (char)I2C_read();',
                  'return true;']),
        # setdev = the per-bus init/probe point: SETWIRE happens HERE,
        # once per bus during the scan (exactly the working driver).
        'setdev': ('bool', 'tc_i2c_setdev',
                   '(uint8_t a, uint8_t bus)',
                   ['I2C_SETWIRE(bus);',
                    'return I2C_SetDevice(a, bus);']),
    }

    CB_MAP = {
        'main':        ('pFUNC_INIT',         True),
        'EverySecond': ('pFUNC_EVERY_SECOND', False),
        'Command':     ('pFUNC_COMMAND',      False),
        'JsonCall':    ('pFUNC_JSON_APPEND',  False),
        'WebCall':     ('pFUNC_WEB_SENSOR',   False),
        'WebUI':       ('pFUNC_WEB_SENSOR',   False),
        'OnExit':      ('pFUNC_DEINIT',       False),
    }

    def gnames(self):
        return {g['name'] for g in self.globals if g['n'] == 'var'}

    def _uses_state(self, node, names):
        # recursively scan AST for an identifier that is a global
        if isinstance(node, dict):
            if node.get('n') == 'id' and node.get('v') in names:
                return True
            return any(self._uses_state(v, names) for v in node.values())
        if isinstance(node, list):
            return any(self._uses_state(x, names) for x in node)
        return False

    def sig(self, f):
        ret = self.cty(f['ret'])
        if f['params']:
            ps = ', '.join(f'{self.cty(p["ty"])} {p["name"]}'
                           + ('[]' if p['arr'] else '') for p in f['params'])
        else:
            ps = 'void'
        return ret, ps

    def render_func(self, f):
        self.build_tenv(f)             # type env for float-op lowering
        ret, ps = self.sig(f)
        names = self.gnames()
        is_init = self.CB_MAP.get(f['name'], (None,))[0] == 'pFUNC_INIT'
        uses_state = self._uses_state(f['body'], names)
        # render body FIRST so call() can flag whether any fixed-ABI
        # (jt[]-routed) call was emitted — those need jt/mt in scope.
        self.fn_abi = False
        self._in_init = is_init        # drives return-value -> initialized
        body = self.st(f['body'], 0)
        self._in_init = False
        # Anything jt[]/mt-routed in plugin mode needs the register
        # stash in scope: ABI j* calls, the soft-float helpers, and
        # FLTC() (PROGMEM via EXEC_OFFSET -> uses mt). ICONST is a plain
        # passthrough and does NOT.
        needs_regs = (uses_state or self.fn_abi or DBG_TRACE or bool(
            re.search(r'\b(fadd|fdiff|fmul|fdiv|ltsf2|gtsf2|eqsf2|tofloat|'
                      r'fixunssfsi|floatunsisf)\(|\bFLTC\(', body)))
        if uses_state:
            # ALLOCMEM (INIT: allocates + returns -1 on OOM) / SETMEMREGS
            # — both also stash jt/mt/mp.
            regs = '  ALLOCMEM\n' if is_init else '  SETMEMREGS\n'
        elif needs_regs:
            # no MODULE_MEMORY, but uses a jt[]/mt-routed construct (or
            # DBG_TRACE's AddLog) -> SETREGS stashes jt/mt.
            regs = '  SETREGS\n'
        else:
            regs = ''
        # crash-trace: AddLog at every function entry so a serial
        # terminal shows the last fn reached before a hard fault.
        if DBG_TRACE:
            regs += ('#ifdef TC2PLUGIN_DBG\n'
                     f'  AddLog(LOG_LEVEL_INFO, '
                     f'PSTR("TCDBG>tc_{f["name"]}"));\n'
                     '#endif\n')
        if is_init:
            # main -> pFUNC_INIT. The plugin-loaded flag is owned by
            # the plugin itself (Init_module() in xdrv_123_plugins.ino
            # delegates it: `#define initialized mt->flags.initialized`)
            # and the hand-written duals set it inside their ALLOCMEM'd
            # Detect(). We follow the same rule but make it CONTRACT-
            # DRIVEN: the value TinyC main() returns is the success
            # signal (> 0 ok / <= 0 fail). Every `return` in main() is
            # rewritten (see st(), n=='return') to set `initialized`
            # from that value and pass it through, all inside this
            # ALLOCMEM-bound scope (mt valid — no gettbl()-NULL fault
            # like a 2nd SETREGS in the dispatcher would hit).
            #
            # Here we only seed the FAIL-SAFE default: `initialized =
            # 0`. So a main() that falls off the end without returning
            # (or returns <= 0) leaves the flag clear -> the loader
            # retries init rather than wrongly believing a missing
            # sensor came up. A guaranteed reg-bind prologue is still
            # forced (ALLOCMEM) because touching `initialized` needs mt
            # even when main() uses no globals; ALLOCMEM also allocs
            # the per-slot MODULE_MEMORY EverySecond()/etc. expect.
            if not re.search(r'\b(ALLOCMEM|SETMEMREGS|SETREGS)\b', regs):
                regs = '  ALLOCMEM\n' + regs
            regs += '  initialized = 0;   // fail-safe; set by main()\'s return\n'
        if regs:                       # inject after the opening '{'
            body = body.replace('{', '{\n' + regs.rstrip(), 1)
        cb = self.CB_MAP.get(f['name'])
        tag = f'  // TinyC {f["name"]}()' + (f' -> {cb[0]}' if cb else '')
        # NOTE: no `static` — RULE 2 (file-scope static is forbidden in
        # plugin mode; the loader needs external linkage for relocation).
        return f'{ret} tc_{f["name"]}({ps}){tag}\n{body}'

    def _render(self):
        NAME = self.mod.upper()
        L = []
        L.append('// ' + '=' * 66)
        L.append(f'// {self.mod}_dual.cpp — generated by tc2plugin (PoC)')
        L.append('// TinyC source -> Tasmota dual-format plugin C++.')
        L.append('// NOT a finished compiler: a subset translator. Lines it')
        L.append('// could not parse are kept as `// TC-PASSTHROUGH:` so')
        L.append('// nothing is silently dropped. Unknown syscalls are')
        L.append('// emitted verbatim + a /* TODO: bind via JMPTBL */ mark.')
        L.append('//')
        L.append('// Plugin-ABI conformance applied: no `static` (RULE 2),')
        L.append('// MODULE_PART forward decl for every fn in the')
        L.append('// PUSH_OPTIONS/MODULE_DESCRIPTOR..MODULE_END block, state')
        L.append('// in per-slot MODULE_MEMORY (ALLOCMEM in INIT, SETMEMREGS')
        L.append('// elsewhere). FLOAT OPS LOWERED: type inference rewrites')
        L.append('// every float +-*/ and < > == to soft-float jt[] helpers')
        L.append('// (fadd/fdiff/fmul/fdiv/ltsf2/gtsf2/eqsf2), int->float via')
        L.append('// tofloat(), float->int via fixunssfsi() — so the C')
        L.append('// compiler emits NO __addsf3/__mulsf3/__floatsisf')
        L.append('// intrinsics the plugin loader cannot bind. CONSTANTS')
        L.append('// POOLED: every float const -> FP_CONST[] PROGMEM via')
        L.append('// FLTC(i); ints wider than 12-bit -> ICONST() — the')
        L.append('// Xtensa literal pool is not relocated in plugin mode.')
        L.append('// STILL NEEDS A HUMAN: real MODULE_DESCRIPTOR')
        L.append('// config keys + chip/bus-id mask; function-scope static')
        L.append('// literals -> per-element (RULE 1); RETMEM on deinit;')
        L.append('// JMPTBL entries for the listed unresolved symbols.')
        L.append('// ' + '=' * 66)
        L.append('#include "tasmota_options.h"')
        L.append('#ifndef BUILD_AS_PLUGIN')
        L.append(f'#  ifdef USE_{NAME}_DUAL_CNV_MOD'
                 '  // _CNV_ = tc2plugin-converted (distinct from any '
                 'hand-written USE_*_DUAL_MOD driver)')
        L.append('#    define BUILD_AS_PLUGIN 1')
        L.append('#  else')
        L.append('#    define BUILD_AS_PLUGIN 0')
        L.append('#  endif')
        L.append('#endif')
        L.append('#include "dual_format_compat.h"')
        L.append('#if BUILD_AS_PLUGIN')
        L.append('#  include "../Tasmota/include/i18n.h"  // D_* labels '
                 '(matches hand-written dual drivers)')
        L.append('#endif')
        L.append('')
        if DBG_TRACE:
            L.append('// Entry-trace instrumentation. Every fn logs '
                     '"TCDBG><name>" on')
            L.append('// entry — invaluable for hard-fault hunting, '
                     'noisy otherwise.')
            L.append('// All trace AddLog()s are #ifdef\'d on this one '
                     'macro: uncomment')
            L.append('// to re-enable (no re-translate needed; edit '
                     'here or build flag).')
            L.append('//#define TC2PLUGIN_DBG 1')
            L.append('')
        if self.defines:
            L.append('// --- #define from TinyC source (object-like) ---')
            L.extend(self.defines)
            L.append('')
        if self.passthru:
            L.append('// --- preprocessor lines not auto-translated ---')
            for p in self.passthru:
                L.append('// TC-PASSTHROUGH: ' + p)
            L.append('')
        # --- state: MODULE_MEMORY (RULE 2: NO file-scope `static`) ---
        L.append('// --- state: TinyC globals -> per-slot MODULE_MEMORY '
                 '(heap; RULE 2 forbids file-scope static in plugin mode) ---')
        L.append(f'#define DUAL_NATIVE_NAME    {self.mod}')
        L.append(f'#define DUAL_NATIVE_STATE_T {self.mod}_state_t')
        L.append('#include "dual_format_native_state.h"')
        L.append('typedef struct {')
        for g in self.globals:
            if g['n'] == 'raw':
                L.append('  // TC-PASSTHROUGH: ' + g['src'].replace('\n', ' '))
                continue
            tagp = '   // persist (move to Settings/UFS in a real port)' \
                   if g.get('persist') else ''
            L.append('  ' + self.g_decl(g) + tagp)
        L.append('  char _ftoa[16];   // tc_ftoa scratch (no file static)')
        L.append('  TWIp *xWire;      // per-instance I2C wire (plugin: set '
                 'by I2C_SETWIRE; matches hand-written dual drivers)')
        L.append('} MODULE_MEMORY;')
        L.append('// translated bodies use bare names; remap to mem->*')
        for g in self.globals:
            if g['n'] == 'var':
                L.append(f'#define {g["name"]} mem->{g["name"]}')
        L.append('#define _ftoa mem->_ftoa')
        L.append('')
        # Pre-render every function NOW so the float-constant pool is
        # fully populated before we emit the PROGMEM table (which must
        # sit at file scope, ahead of the code that uses FLTC()).
        func_blocks = [self.render_func(d) for d in self.decls
                       if d['n'] == 'func']
        if self.fpool:
            L.append('// Constant pool — floats can\'t be Xtensa immediates '
                     'and the')
            L.append('// literal pool is not relocated in plugin mode; this '
                     'PROGMEM')
            L.append('// array IS loaded (RULE 3). FLTC(i) = native array '
                     'index /')
            L.append('// plugin EXEC_OFFSET PROGMEM read. (>12-bit ints use '
                     'ICONST.)')
            L.append(f'const float FP_CONST_{NAME}[] PROGMEM = {{ '
                     + ', '.join(self.fpool) + ' };')
            L.append(f'#define DUAL_FLTC_TABLE FP_CONST_{NAME}')
            L.append('#include "dual_format_fltc.h"')
            L.append('')
        if self.need_lstr:
            # TinyC LGetString(index,dst) == the VM's fixed 21-entry
            # localized-label table. Replicate it as a file-scope
            # PROGMEM '|' table (RULE 3 safe), called via the working-
            # driver GetTextIndexed(dst,size,idx,GSTR(tbl)) pattern.
            # Labels/order MUST match xdrv_124_tinyc_vm.h SYS_LGETSTRING.
            L.append('// LGetString table — mirrors the TinyC VM '
                     'SYS_LGETSTRING D_* set (index 0..20)')
            L.append('const char tc_lstr_tbl[] PROGMEM =')
            L.append('  D_TEMPERATURE "|" D_HUMIDITY "|" D_PRESSURE "|" '
                     'D_DEWPOINT "|" D_CO2 "|"')
            L.append('  D_ECO2 "|" D_TVOC "|" D_VOLTAGE "|" D_CURRENT "|" '
                     'D_POWERUSAGE "|"')
            L.append('  D_POWER_FACTOR "|" D_ENERGY_TODAY "|" '
                     'D_ENERGY_YESTERDAY "|" D_ENERGY_TOTAL "|"')
            L.append('  D_FREQUENCY "|" D_ILLUMINANCE "|" D_DISTANCE "|" '
                     'D_MOISTURE "|" D_LIGHT "|"')
            L.append('  D_SPEED "|" D_ABSOLUTE_HUMIDITY;')
            L.append('')
        # --- canonical descriptor block: MODULE_PART for EVERY function,
        #     in source order, BEFORE the definitions ---
        L.append('// Plugin descriptor block — the loader walks '
                 'MODULE_DESCRIPTOR..MODULE_END')
        L.append('// for every MODULE_PART decl (source order). Do not '
                 'split with #if.')
        L.append('PUSH_OPTIONS')
        L.append(f'MODULE_DESCRIPTOR("{NAME}", MODULE_TYPE_SENSOR, '
                 '1 << 16 | 124,')
        L.append('                  "", 0, "", 0, "", 0, "", 0)'
                 '  // TODO: real config keys / chip-id mask')
        L.append('MODULE_PART const char* tc_ftoa(float v);')
        for h in sorted(self.i2c_helpers):
            ret, nm, ps, _ = self.I2C_HELPERS[h]
            L.append(f'MODULE_PART {ret} {nm}{ps};')
        for d in self.decls:
            if d['n'] == 'func':
                ret, ps = self.sig(d)
                L.append(f'MODULE_PART {ret} tc_{d["name"]}({ps});')
        L.append('#if BUILD_AS_PLUGIN')
        L.append('MODULE_PART int32_t mod_func_execute(uint32_t function);')
        L.append('#endif')
        L.append('MODULE_END')
        L.append('')
        # ftoa shim — MODULE_PART, uses MODULE_MEMORY scratch (no static)
        L.append('const char* tc_ftoa(float v) {')
        L.append('  SETMEMREGS')
        if DBG_TRACE:
            L.append('#ifdef TC2PLUGIN_DBG')
            L.append('  AddLog(LOG_LEVEL_INFO, PSTR("TCDBG>tc_ftoa"));')
            L.append('#endif')
        L.append('  dtostrfd(v, 3, _ftoa); return _ftoa;')
        L.append('}')
        L.append('')
        # generated I2C shims (raw I2C_* on the wire established by
        # setdev's I2C_SETWIRE — see I2C_HELPERS comment)
        for h in sorted(self.i2c_helpers):
            ret, nm, ps, body = self.I2C_HELPERS[h]
            L.append(f'{ret} {nm}{ps} {{')
            L.append('  SETREGS')
            if DBG_TRACE:
                L.append('#ifdef TC2PLUGIN_DBG')
                L.append(f'  AddLog(LOG_LEVEL_INFO, PSTR("TCDBG>{nm}"));')
                L.append('#endif')
            for ln in body:
                L.append('  ' + ln)
            L.append('}')
            L.append('')
        # function definitions (no `static`) — pre-rendered above so the
        # float pool was complete before the PROGMEM table was emitted.
        for blk in func_blocks:
            L.append(blk)
            L.append('')
        L.append('int32_t mod_func_execute(uint32_t function) {')
        if DBG_TRACE:
            L.append('#ifdef TC2PLUGIN_DBG')
            L.append('  { SETREGS AddLog(LOG_LEVEL_INFO, '
                     'PSTR("TCDBG>mod_func_execute fn=%u"), '
                     '(unsigned)function); }')
            L.append('#endif')
        L.append('  switch (function) {')
        seen = set()
        for nm, (pf, is_init) in self.CB_MAP.items():
            if nm not in self.funcs or pf in seen or pf == 'pFUNC_DEINIT':
                continue
            seen.add(pf)
            if is_init:
                # main -> pFUNC_INIT. The plugin-loaded flag is set
                # INSIDE tc_main() (see render_func), within its
                # ALLOCMEM-bound scope — NOT here. A 2nd standalone
                # SETREGS after tc_main() returned faulted: gettbl() at
                # that point returned NULL -> mt->jt / *asettings
                # null-deref (Exception 7 / EXCVADDR 0, deterministic on
                # real HW). The hand-written duals likewise set
                # `initialized` inside their ALLOCMEM'd Detect(), never
                # via a fresh reg-bind in the dispatch switch.
                L.append(f'    case {pf}: return tc_{nm}();')
            else:
                L.append(f'    case {pf}: tc_{nm}(); break;')
        # pFUNC_DEINIT must ALWAYS free the per-slot MODULE_MEMORY
        # (RETMEM), whether or not the script defined OnExit().
        L.append('    case pFUNC_DEINIT: {')
        if 'OnExit' in self.funcs:
            L.append('      tc_OnExit();')
        L.append('      SETMEMREGS RETMEM        // free MODULE_MEMORY')
        L.append('      break;')
        L.append('    }')
        L.append('    default: break;')
        L.append('  }')
        L.append('  return 0;')
        L.append('}')
        L.append('')
        L.append('// ' + '-' * 64)
        L.append('// Syscall resolution summary')
        L.append('// ' + '-' * 64)
        if self.resolved:
            L.append(f'// {len(self.resolved)} call(s) mapped to the FIXED '
                     'plugin ABI (module_defines.h /')
            L.append('// jt[]) — these compile + run as-is in both native '
                     'and plugin:')
            L.append('//   ' + ', '.join(sorted(self.resolved)))
        if self.vm_internal:
            L.append('// ' + '-' * 60)
            L.append('// TinyC VM constructs with NO jump-table function — '
                     'must be')
            L.append('// reimplemented by hand (not a binding step):')
            for n in sorted(self.vm_internal):
                L.append(f'//   - {n}: {self.vm_internal[n]}')
        if self.todo:
            L.append('// ' + '-' * 60)
            L.append('// Genuinely unknown — not in the ABI table yet; '
                     'verify a jt[]')
            L.append('// entry exists (it is fixed) and add to '
                     'syscall_abi_map.py:')
            for t in sorted(set(self.todo)):
                L.append(f'//   - {t}')
        return '\n'.join(L)


_PP_DEFINE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+(.*\S)\s*$')


def extract_pp(src):
    """Pull C-preprocessor lines out before lexing (the TinyC lexer is
    expression-level). Object-like `#define NAME value` are kept as real
    C++ #defines (the compiler resolves them); everything else (#include
    of .tc, #ifdef, …) becomes a // TC-PASSTHROUGH note so nothing is
    silently dropped. Each removed line is blanked to preserve layout."""
    defines, passthru, out = [], [], []
    for ln in src.split('\n'):
        s = ln.lstrip()
        if s.startswith('#'):
            m = _PP_DEFINE.match(ln)
            if m and '(' not in m.group(1):          # object-like only
                defines.append(f'#define {m.group(1)} {m.group(2)}')
            else:
                passthru.append(s)
            out.append('')                            # keep line numbers
        else:
            out.append(ln)
    return '\n'.join(out), defines, passthru


def translate(src, modname='sml_emu'):
    clean, defines, passthru = extract_pp(src)
    toks = lex(clean)
    ast = Parser(toks, clean).parse()
    return Emitter(ast, modname, defines, passthru).emit()


# --------------------------------------------------------------------
# Web UI (single page, stdlib http.server)
# --------------------------------------------------------------------
PAGE = '''<!doctype html><html><head><meta charset="utf-8">
<title>tc2plugin — TinyC → Plugin (PoC)</title><style>
 body{margin:0;font:13px/1.4 -apple-system,Menlo,monospace;background:#1e1e1e;color:#ddd}
 header{background:#252526;padding:10px 14px;border-bottom:1px solid #333;display:flex;align-items:center;gap:14px}
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
 /* token colors (VSCode-dark-ish) */
 .tk-c{color:#6a9955}.tk-s{color:#ce9178}.tk-n{color:#b5cea8}
 .tk-k{color:#569cd6}.tk-t{color:#4ec9b0}.tk-p{color:#c586c0}
 .err{color:#f48771;padding:10px 12px;white-space:pre-wrap;font-size:12px}
</style></head><body>
<header><b>tc2plugin</b><span>TinyC&nbsp;→&nbsp;plugin C++ · PoC</span>
<label for="fp" style="background:#37373d;padding:7px 12px;border-radius:4px;cursor:pointer">📂 Open .tc…</label>
<input id="fp" type="file" accept=".tc,.c,.h,.txt" style="display:none">
<select id="ex" style="background:#37373d;color:#ddd;border:0;padding:7px;border-radius:4px;cursor:pointer">
 <option value="">— examples —</option></select>
<button id="go">Translate ▶</button>
<button id="cc" style="background:#36c;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Enable USE_<MOD>_DUAL_MOD in user_config_override.h, then fork: pio run -e tasmota32-plugin">Compile ⚙</button>
<button id="sv" style="background:#393;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Download the right pane verbatim as xsns_198_<mod>_cnv_dual.cpp (saves hand-edits)">Save ▼</button>
<button id="rs" disabled style="background:#777;color:#fff;border:0;padding:8px 16px;border-radius:4px;cursor:pointer" title="Compile replaces the pane with the build log. After reading it, toggle back to your edited source (and back to the log).">Restore ↩</button>
<button id="quit" style="background:#a33;color:#fff;border:0;padding:8px 14px;border-radius:4px;cursor:pointer;margin-left:auto">Exit ✕</button>
<span id="st"></span></header>
<div class="wrap">
 <div class="col"><div class="lbl">TinyC source (.tc)</div>
   <div class="ed"><pre id="inhl" aria-hidden="true"></pre>
   <textarea id="in" spellcheck="false">__TC__</textarea></div></div>
 <div class="col" id="outwrap"><div class="lbl">Generated plugin C++ (xsns_*_dual.cpp shape)</div>
   <div class="ed"><pre id="outhl" aria-hidden="true"></pre>
   <textarea id="out" spellcheck="false" title="Editable: hand-tweak the generated C++ then press Compile. Translate ▶ regenerates and discards edits."></textarea></div></div>
</div>
<script>
const go=document.getElementById('go'),inp=document.getElementById('in'),
      out=document.getElementById('out'),st=document.getElementById('st'),
      inhl=document.getElementById('inhl'),outhl=document.getElementById('outhl');
let curName='__NAME__';
// --- dependency-free syntax highlighter (C / TinyC) ---
const KW=new Set(('int float char void bool if else while for return persist '
 +'break continue struct typedef const static unsigned signed short long '
 +'double sizeof switch case default goto enum union volatile').split(' '));
const TY=new Set(('int32_t uint32_t int16_t uint16_t int8_t uint8_t int64_t '
 +'uint64_t size_t MODULE_MEMORY MOD_RESULT').split(' '));
const MAC=new Set(('PSTR FLTC ICONST SETREGS SETMEMREGS ALLOCMEM RETMEM '
 +'MODULE_PART MODULE_END MODULE_DESCRIPTOR PUSH_OPTIONS PULL_OPTIONS '
 +'fadd fdiff fmul fdiv ltsf2 gtsf2 eqsf2 tofloat fixunssfsi floatunsisf '
 +'AddLog LOG_LEVEL_INFO').split(' '));
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
 if(v.length>40000){pre.textContent=v;}        // huge build log: plain+fast
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
async function tr(){
  st.textContent='translating…';
  try{
    const r=await fetch('/translate?name='+encodeURIComponent(curName),
                        {method:'POST',body:inp.value});
    const j=await r.json();
    out.value=j.ok?j.code:('// TRANSLATION ERROR\\n// '+j.error);
    st.textContent=j.ok?('ok · '+j.lines+' lines · '+curName):'error';
  }catch(e){out.value='// '+e;st.textContent='error';}
  // fresh generated source -> any prior compile-log restore is stale
  savedSrc=null;savedLog=null;showingLog=false;
  if(typeof setRestore==='function')setRestore(false);
  pOut();
}
go.onclick=tr;
const cc=document.getElementById('cc'),rs=document.getElementById('rs');
// pane-restore state: Compile overwrites the pane with the build log;
// keep the pre-compile source so it can be toggled back after the
// log is read (and toggled to the log again).
let savedSrc=null,savedLog=null,showingLog=false;
function setRestore(on){rs.disabled=!on;
  rs.style.background=on?'#a60':'#777';
  rs.textContent=on&&showingLog?'Restore ↩':(on?'Show log ↪':'Restore ↩');}
rs.onclick=()=>{
  if(savedSrc==null)return;
  if(showingLog){savedLog=out.value;out.value=savedSrc;showingLog=false;
    st.textContent='source restored — edit & Compile again, or Show log';}
  else{savedSrc=out.value;out.value=savedLog!=null?savedLog:out.value;
    showingLog=true;st.textContent='showing build log';}
  pOut();setRestore(true);};
cc.onclick=async()=>{
  if(!out.value.trim()||out.value.startsWith('// TRANSLATION ERROR')){
    st.textContent='translate first';return;}
  const code=out.value;
  savedSrc=code;savedLog=null;showingLog=true;setRestore(false);
  cc.disabled=true;st.textContent='compiling (forking pio)…';
  out.value='# Compile: enabling USE_'+curName.replace(/\\.[^.]*$/,'')
            .replace(/\\W/g,'_').toUpperCase()+'_DUAL_MOD then '
            +'pio run -e tasmota32-plugin …\\n';
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
  if(!out.value.trim()||out.value.startsWith('// TRANSLATION ERROR')
     ||out.value.startsWith('# Compile')){
    st.textContent='nothing to save — translate first';return;}
  const mod=curName.replace(/\\.[^.]*$/,'').replace(/\\W/g,'_')
            .replace(/^_+|_+$/g,'').toLowerCase()||'tc_module';
  const fn='xsns_198_'+(/^\\d/.test(mod)?'m_'+mod:mod)+'_cnv_dual.cpp';
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
  for(const n of j.files){const o=document.createElement('option');
    o.value=n;o.textContent=n;ex.appendChild(o);}}catch(e){}})();
document.getElementById('quit').onclick=async()=>{
  st.textContent='shutting down…';
  try{await fetch('/quit',{method:'POST'});}catch(e){}
  document.body.innerHTML='<div style="padding:40px;font:16px -apple-system">'
    +'tc2plugin server stopped. You can close this tab.</div>';
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

    # ---- Compile: enable module in override, fork pio, stream log ----
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

        use = f'USE_{NAME}_DUAL_CNV_MOD'
        bak = OVERRIDE + '.tc2plugin.bak'
        slot = os.path.join(REPO, 'tasmota', 'Plugins',
                            f'xsns_198_{mod}_cnv_dual.cpp')
        bp = os.path.join(REPO, 'tasmota', 'Plugins', 'build_plugin.py')
        try:
            w(f"# tc2plugin compile — '{mod}'  gate {use}\n")
            # 1) drop the generated dual .cpp into Plugins/. NO native
            #    xsns shim: build_plugin.py compiles it as the standalone
            #    plugin TU (the broken shim caused the module-header
            #    double-include collision).
            with open(slot, 'w') as f:
                f.write(cpp_src)
            w(f"# wrote {os.path.relpath(slot, REPO)}\n")
            # 2) build_plugin.py needs a (commented) #define for the
            #    target inside the PLUGIN_DEFINES sentinel section so it
            #    can activate it. Insert if absent (restored in finally).
            shutil.copy2(OVERRIDE, bak)
            txt = open(OVERRIDE, encoding='utf-8', errors='replace').read()
            if use not in txt:
                end = '// >>> PLUGIN_DEFINES_END'
                txt = txt.replace(end, f'//#define {use}\n{end}', 1)
                open(OVERRIDE, 'w', encoding='utf-8').write(txt)
                w(f"# added //#define {use} to PLUGIN_DEFINES section\n")
            # 3) delegate to the canonical single-plugin builder — it
            #    activates the gate, picks env tasmota32-plugin, builds,
            #    extracts the .bin, and restores override itself.
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
            # 4) report extracted .bin(s)
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
                w("# build OK but no matching .bin found — check "
                  "build_plugin.py output above\n")
            else:
                w("# build FAILED — see errors above\n")
        except Exception as e:
            w(f"\n# tc2plugin compile error: {type(e).__name__}: {e}\n")
        finally:
            # build_plugin.py restores override to the state it backed up
            # (with our commented line); our bak removes that line too.
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
                fs = sorted(f for f in os.listdir(EXAMPLES_DIR)
                            if f.endswith('.tc'))
            except OSError:
                fs = []
            return self._json({'files': fs})
        if u.path == '/file':
            q = parse_qs(u.query).get('path', [''])[0]
            # path-safe: only a bare filename inside EXAMPLES_DIR
            name = os.path.basename(q)
            full = os.path.realpath(os.path.join(EXAMPLES_DIR, name))
            if (not full.startswith(EXAMPLES_DIR + os.sep)
                    or not os.path.isfile(full)):
                return self._json({'ok': False, 'error': 'not found'})
            try:
                with open(full, encoding='utf-8', errors='replace') as fh:
                    return self._json({'ok': True, 'text': fh.read()})
            except OSError as e:
                return self._json({'ok': False, 'error': str(e)})
        body = (PAGE.replace('__TC__', html.escape(DEFAULT_TC))
                    .replace('__NAME__', DEFAULT_NAME)).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        if urlparse(self.path).path == '/quit':
            self._json({'ok': True})
            # exit from a separate thread so the response flushes first;
            # os._exit guarantees the port is freed (no zombie server).
            threading.Timer(0.3, lambda: os._exit(0)).start()
            return
        n = int(self.headers.get('Content-Length', 0))
        src = self.rfile.read(n).decode('utf-8', 'replace')
        raw = parse_qs(urlparse(self.path).query).get('name', [''])[0]
        # sanitise filename -> C identifier (sht31.tc -> sht31)
        modname = re.sub(r'\.(tc|c|h|txt)$', '', os.path.basename(raw))
        modname = re.sub(r'\W', '_', modname).strip('_').lower() or 'tc_module'
        if urlparse(self.path).path == '/compile':
            return self.do_compile(src, modname)
        mod = re.sub(r'\.(tc|c|h|txt)$', '', os.path.basename(raw))
        mod = re.sub(r'\W', '_', mod).strip('_').lower() or 'tc_module'
        if mod[0].isdigit():
            mod = 'm_' + mod
        try:
            code = translate(src, mod)
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
        # headless: translate stdin -> stdout (handy for testing)
        print(translate(sys.stdin.read()))
        return
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(('127.0.0.1', LISTEN_PORT), H) as s:
        url = f'http://127.0.0.1:{LISTEN_PORT}/'
        print(f'tc2plugin running → {url}  (Ctrl-C to stop)')
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
        try:
            s.serve_forever()
        except KeyboardInterrupt:
            print('\nstopped.')


if __name__ == '__main__':
    main()
