#!/usr/bin/env python3
"""gen_picotts.py - derive the BinPlugin sources of the PicoTTS engine from the pristine tree.

    python3 tasmota/Plugins/picotts/tools/gen_picotts.py [--check]

Input : lib/libesp32_div/pico/lib/*.c|*.h and esp_picorsrc.c (never modified)
Output: tasmota/Plugins/picotts/engine/*.h      patched copies of the engine headers
        tasmota/Plugins/picotts/engine/picotts_engine_c.h   ONE amalgamated translation unit
        tasmota/Plugins/picotts/engine/picotts_plugin_strings.h   text blob + offsets

Why a generator and not hand-edited copies: the engine is 41 kLOC of third party C
(SVOX Pico, Apache 2.0). Everything a BinPlugin cannot do is a mechanical edit, so it is
re-derived from the pristine sources and stays diffable.

What a BinPlugin cannot do (tasmota/Plugins/AUDIT_2026-09-26.md, matter/PLUGIN_PLAN.md):
  * every function must sit in the module section      -> MODULE_PART in front of each definition
  * no string literals (they land in host .rodata)      -> PICO_S(n): pointer into a RAM copy of one blob
  * no inline libc / libgcc calls                        -> picotts_plugin_pal.h (own picopal layer)
  * no double / float division intrinsics                -> float typedef + FDIV() (jump table)
  * C++ keywords used as identifiers (unity build is C++) -> this->thiz, class->klass
  * 58 macros are #defined with different values in several files (unity build) -> #undef at file end
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLUG = os.path.dirname(HERE)                                   # tasmota/Plugins/picotts
REPO = os.path.abspath(os.path.join(PLUG, "..", "..", ".."))
PICO = os.path.join(REPO, "lib", "libesp32_div", "pico")
OUT_INC = os.path.join(PLUG, "engine")
OUT_SRC = os.path.join(PLUG, "engine")

# engine files in link order; picopal.c is replaced by picotts_plugin_pal.h, picodbg.c is debug only,
# picorsrc.c is pulled in through esp_picorsrc.c exactly like upstream does
SKIP = {"picopal.c", "picodbg.c", "picorsrc.c"}

# static helpers that exist in two files (a unity build has ONE namespace)
RENAMES = {
    "picospho.c": {"getNextPosSym": "spho_getNextPosSym"},
    # a second, different struct with the same tag (picoacph.c has the original)
    "picocep.c": {"picoacph_headx_t": "picocep_headx_t"},
}
# identifiers that are C++ keywords
KEYWORDS = {"this": "thiz", "class": "klass"}

# file specific textual edits: (file, old, new)  - each must match exactly once
EDITS = [
    # a function-local static counter that only feeds a debug message
    ("picosig.c", "static int nFrame = 0;", "int nFrame = 0;"),
    # C++ does not increment enums
    ("picotok.c", "for (mId = MIDummyStart; mId <= MIDummyEnd; mId++) {",
                  "for (mId = MIDummyStart; mId <= MIDummyEnd; mId = (MarkupId)(mId + 1)) {"),
    # esp_picorsrc.c: pointer arithmetic on const void* (GNU C only) and a char array initialised
    # from a literal (literals become PICO_S pointers)
    ("esp_picorsrc.c", 'const char marker[] = " (C) SVOX AG ";', 'const char *marker = " (C) SVOX AG ";'),
    ("esp_picorsrc.c", "i < sizeof(marker) -1;", "i < picopal_strlen((const picopal_char *)marker);"),
    ("esp_picorsrc.c", "pico_status_t esp_pico_loadResource(pico_System sys, const void *raw, pico_Resource *outResource)",
                       "pico_status_t esp_pico_loadResource(pico_System sys, const char *raw, pico_Resource *outResource)"),
]


# Float arithmetic. The plugin must not emit libgcc calls: Xtensa has no float divide instruction, so
# every `a / b` on floats becomes a call of libgcc's __divsf3 - an address of the HOST. Those sites go
# through the jump table (PICO_FDIV, jt[39]); sqrt/exp likewise (PICO_SQRTF/PICO_EXPF). The engine
# computes these in double; single precision is enough for speech (tested on the host, README).
# (file, regex, replacement, expected number of matches)
FLOAT_EDITS = [
    ("picofftsg.c", r"return \(picoos_single\)sqrt\(\(double\)E/16\.0\)/m2;",
     "return PICO_FDIV((picoos_single)PICO_SQRTF(PICO_FDIV((float)E, 16.0f)), m2);", 1),
    ("picopam.c", r"lfz = \(picoos_single\) lfum / \(picoos_single\) lfivar;",
     "lfz = PICO_FDIV((picoos_single) lfum, (picoos_single) lfivar);", 1),
    ("picopam.c", r"f0avg /= \(picoos_single\) numstates;",
     "f0avg = PICO_FDIV(f0avg, (picoos_single) numstates);", 1),
    ("picopam.c", r"f_round\(f0avg / f0quant\)", "f_round(PICO_FDIV(f0avg, f0quant))", 1),
    ("picopam.c", r"f_round\(fDur / durquant1\)", "f_round(PICO_FDIV(fDur, durquant1))", 1),
    ("picopam.c", r"f_round\(fDur / durquant2\)", "f_round(PICO_FDIV(fDur, durquant2))", 1),
    ("picopam.c", r"\(fDur / \(picoos_single\) 10\.0f\)", "PICO_FDIV(fDur, (picoos_single) 10.0f)", 1),
    ("picopam.c", r"\(f0avg / \(picoos_single\) 10\.0f\)", "PICO_FDIV(f0avg, (picoos_single) 10.0f)", 1),
    ("picopam.c", r"\(picoos_single\) nValue / \(picoos_single\) (100|1000)\.0f",
     r"PICO_FDIV((picoos_single) nValue, (picoos_single) \1.0f)", 2),
    ("picopam.c", r"\(1\.0f / fValue\)", "PICO_FDIV(1.0f, fValue)", 3),
    ("picopam.c", r"\(picoos_single\) exp\(\(double\) lfz\)", "(picoos_single) PICO_EXPF(lfz)", 1),
    ("picosig.c", r"\(\(picoos_single\) tmp_uint16\s*/ sig_subObj->scmeanLFZ\)",
     "PICO_FDIV((picoos_single) tmp_uint16, sig_subObj->scmeanLFZ)", 1),
    ("picosig.c", r"\(picoos_single\) \(\(tmp_uint16\s*& 0x01\) \* 8 \+ \(tmp_uint16 & 0x0e\) / 2\)\s*/ \(picoos_single\) 15\.0f",
     "PICO_FDIV((picoos_single) ((tmp_uint16 & 0x01) * 8 + (tmp_uint16 & 0x0e) / 2), (picoos_single) 15.0f)", 1),
    ("picosig.c", r"\(picoos_single\) tmp_uint16\s*/ sig_subObj->scmeanLFZ;",
     "PICO_FDIV((picoos_single) tmp_uint16, sig_subObj->scmeanLFZ);", 1),
    ("picosig.c", r"\(picoos_single\) exp\(\s*\(picoos_single\) sig_subObj->sig_inner\.F0_p\)",
     "(picoos_single) PICO_EXPF((picoos_single) sig_subObj->sig_inner.F0_p)", 1),
    ("picosig.c", r"\(picoos_single\) n_value\s*/ \(picoos_single\) (100|1000)\.0f",
     r"PICO_FDIV((picoos_single) n_value, (picoos_single) \1.0f)", 2),
    ("picosig2.c", r"\(picoos_single\) Fs\s*/ \(picoos_single\) (sig_inObj->Fuv_p|F0)\)",
     r"PICO_FDIV((picoos_single) Fs, (picoos_single) \1))", 2),
    ("picosig2.c", r"sqrt\(\(double\) Fs\s*/ \(hop \* sig_inObj->(Fuv_p|F0_p)\)\)",
     r"PICO_SQRTF(PICO_FDIV((float) Fs, (hop * sig_inObj->\1)))", 2),
]

# Local arrays with constant initialisers: GCC materialises them as a copy from .rodata, i.e. from an
# address of the plugin HOST. Spell them out as element assignments instead.
FLOAT_EDITS += [
    ("picopam.c", r"(?s)picoos_uint16 tmp_weights\[PICOPAM_PWIDX_SIZE\]\[PICOPAM_MAX_STATES_PER_PHONE\] = \{.*?\};",
     "picoos_uint16 tmp_weights[PICOPAM_PWIDX_SIZE][PICOPAM_MAX_STATES_PER_PHONE];\n"
     "        {\n"
     + "".join("        tmp_weights[%d][%d] = %d;\n" % (r, c, v)
               for r, row in enumerate([(10, 10, 10, 10, 1), (1, 4, 8, 4, 1), (1, 4, 8, 4, 1), (1, 10, 10, 10, 10), (1, 1, 1, 1, 1)])
               for c, v in enumerate(row))
     + "        }", 1),
]


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


# ---------------------------------------------------------------------------------------------
# lexer: split into code / comments / strings / preprocessor lines
# ---------------------------------------------------------------------------------------------
class Src:
    """normalised source: comments removed, string literals and preprocessor lines parked in tables
    so that brace matching works on plain code"""

    def __init__(self, text):
        self.strs = []       # string literal runs (raw source text, quotes included)
        self.chrs = []       # character literals (parked: they may contain braces)
        self.pps = []        # preprocessor logical lines
        self.code = self._lex(text)

    def _lex(self, s):
        out = []
        i, n = 0, len(s)
        bol = True           # at beginning of a logical line (only blanks before)
        while i < n:
            c = s[i]
            if c == "/" and s.startswith("/*", i):
                j = s.find("*/", i + 2)
                j = n if j < 0 else j + 2
                out.append("\n" * s.count("\n", i, j) or " ")
                i = j
                continue
            if c == "/" and s.startswith("//", i):
                j = s.find("\n", i)
                i = n if j < 0 else j
                continue
            if c == "#" and bol:
                j = i
                while True:                       # logical line with continuations and comments
                    k = s.find("\n", j)
                    k = n if k < 0 else k
                    seg = s[j:k]
                    if seg.rstrip().endswith("\\") and k < n:
                        j = k + 1
                        continue
                    j = k
                    break
                line = s[i:j]
                line = re.sub(r"/\*.*?\*/", " ", line, flags=re.S)
                line = re.sub(r"//[^\n]*", "", line)
                self.pps.append(line)
                out.append("\x03%d\x04" % (len(self.pps) - 1))
                i = j
                continue
            if c == '"':
                j = i + 1
                while j < n and s[j] != '"':
                    j += 2 if s[j] == "\\" else 1
                j += 1
                self.strs.append(s[i:j])
                out.append("\x01%d\x02" % (len(self.strs) - 1))
                i = j
                bol = False
                continue
            if c == "'":
                j = i + 1
                while j < n and s[j] != "'":
                    j += 2 if s[j] == "\\" else 1
                j += 1
                self.chrs.append(s[i:j])
                out.append("\x05%d\x06" % (len(self.chrs) - 1))
                i = j
                bol = False
                continue
            if c == "\n":
                bol = True
            elif not c.isspace():
                bol = False
            out.append(c)
            i += 1
        return "".join(out)


# ---------------------------------------------------------------------------------------------
# transformations
# ---------------------------------------------------------------------------------------------
def mark_functions(code):
    """insert MODULE_PART in front of every function definition (brace depth 0, '{' after ')')"""
    out = []
    depth = 0
    decl = 0             # start of the current top level declaration in `out` joined positions
    i, n = 0, len(code)
    # work on a list of chars so insert positions stay valid
    buf = list(code)
    inserts = []
    start = 0
    last = ""
    while i < n:
        c = buf[i]
        if c == "\x03":                   # preprocessor line placeholder: new declaration starts after it
            j = code.find("\x04", i)
            i = j + 1
            if depth == 0:
                start = i
            continue
        if c == "{":
            if depth == 0 and last == ")":
                k = start
                while k < i and buf[k].isspace():
                    k += 1
                inserts.append(k)
            depth += 1
            last = "{"
        elif c == "}":
            depth -= 1
            if depth == 0:
                start = i + 1
            last = "}"
        elif c == ";":
            if depth == 0:
                start = i + 1
            last = ";"
        elif not c.isspace():
            last = c
        i += 1
    for k in reversed(inserts):
        buf.insert(k, "MODULE_PART ")
    return "".join(buf), len(inserts)


class Strings:
    """de-duplicated text blob shared by all files"""

    def __init__(self):
        self.ids = {}
        self.items = []

    @staticmethod
    def decode(lits):
        out = bytearray()
        for body in re.findall(r'"((?:\\.|[^"\\\n])*)"', lits):
            raw, i = body.encode("utf-8"), 0
            esc = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, '"': 34, "'": 39, "a": 7, "b": 8, "f": 12, "v": 11, "?": 63}
            while i < len(raw):
                c = raw[i]
                if c != 0x5C:
                    out.append(c)
                    i += 1
                    continue
                nx = chr(raw[i + 1])
                if nx == "x":
                    m = re.match(rb"[0-9a-fA-F]+", raw[i + 2:])
                    out.append(int(m.group(0), 16) & 0xFF)
                    i += 2 + len(m.group(0))
                elif nx in "01234567":
                    m = re.match(rb"[0-7]{1,3}", raw[i + 1:])
                    out.append(int(m.group(0), 8) & 0xFF)
                    i += 1 + len(m.group(0))
                else:
                    out.append(esc.get(nx, ord(nx)))
                    i += 2
        return bytes(out)

    def get(self, lits):
        data = self.decode(lits)
        if data not in self.ids:
            self.ids[data] = len(self.items)
            self.items.append(data)
        return self.ids[data]


def restore(src, code, strings, functions=False):
    """put string literals back (as PICO_S(id)), character literals and preprocessor lines"""
    def run(m):
        lits = "".join(src.strs[int(x)] for x in re.findall(r"\x01(\d+)\x02", m.group(0)))
        return "PICO_S(%d)" % strings.get(lits)

    def pp_sub(m):
        line = src.pps[int(m.group(1))]
        if re.match(r"\s*#\s*(include|error|pragma|line)\b", line):
            return line
        line = re.sub(r'(?:"(?:\\.|[^"\\\n])*"\s*)+', lambda mm: "PICO_S(%d)" % strings.get(mm.group(0)), line)
        if functions and re.match(r"\s*#\s*define\b", line):
            line = float_fix(line)
        return line

    code = re.sub(r"(?:\x01\d+\x02\s*)+", run, code)          # adjacent literals are one literal
    code = re.sub(r"\x05(\d+)\x06", lambda m: src.chrs[int(m.group(1))], code)
    code = re.sub(r"\x03(\d+)\x04", pp_sub, code)
    return code


def float_fix(text):
    """whatever double is left: casts, math functions, floating point constants"""
    text = re.sub(r"\(\s*double\s*\)", "(float)", text)
    text = re.sub(r"\bsqrt\s*\(", "PICO_SQRTF(", text)
    text = re.sub(r"\bexp\s*\(", "PICO_EXPF(", text)
    return re.sub(r"(?<![\w.])(\d+\.\d*(?:[eE][-+]?\d+)?|\.\d+(?:[eE][-+]?\d+)?|\d+[eE][-+]?\d+)(?![\w.])(?!f)",
                  lambda m: m.group(1) + "f", text)


def kb_arrays(pico_dir):
    """PICOKNOW_KBID_*_ARRAY macros of picoknow.h -> {name: [element, ...]}"""
    text = read(os.path.join(pico_dir, "lib", "picoknow.h"))
    out = {}
    for m in re.finditer(r"#define\s+(PICOKNOW_KBID_\w+_ARRAY)\s*\{(.*?)\}", text, re.S):
        items = [x.strip() for x in m.group(2).replace("\\", " ").split(",") if x.strip()]
        out[m.group(1)] = items
    return out


def expand_kb_arrays(text, arrays):
    """`T name[N] = PICOKNOW_KBID_X_ARRAY;` is materialised by GCC as a copy from .rodata (an address of
    the plugin host). Spell it out as element assignments."""
    def sub(m):
        typ, name, size, macro = m.group(1), m.group(2), m.group(3), m.group(4)
        items = arrays[macro]
        return "%s %s[%s];\n    %s" % (typ, name, size, " ".join("%s[%d] = %s;" % (name, i, it) for i, it in enumerate(items)))
    return re.subn(r"(picoknow_kb_id_t)\s+(\w+)\[(\w+)\]\s*=\s*(PICOKNOW_KBID_\w+_ARRAY)\s*;", sub, text)


FUNC_NAMES = set()
MACRO_NAMES = set()      # names that are macros somewhere (e.g. picotrns_printSolution -> NULL without debug)
WRAP_POINTERS = False


def func_names(code):
    """names of the functions defined in `code` (the identifier before the first '(' of a definition)"""
    names = set()
    depth = 0
    i, n = 0, len(code)
    start = 0
    last = ""
    while i < n:
        c = code[i]
        if c == "\x03":
            i = code.find("\x04", i) + 1
            if depth == 0:
                start = i
            continue
        if c == "{":
            if depth == 0 and last == ")":
                decl = code[start:i]
                par = decl.find("(")
                m = re.search(r"([A-Za-z_]\w*)\s*$", decl[:par]) if par > 0 else None
                if m:
                    names.add(m.group(1))
            depth += 1
            last = "{"
        elif c == "}":
            depth -= 1
            if depth == 0:
                start = i + 1
            last = "}"
        elif c == ";":
            if depth == 0:
                start = i + 1
            last = ";"
        elif not c.isspace():
            last = c
        i += 1
    return names


def wrap_function_pointers(code, names):
    """a function name used as a value (not called) is an absolute address of the LINKED module; it
    needs the module's execution offset to be valid at run time -> PICO_FP(name)"""
    if not names:
        return code
    pat = re.compile(r"(?<![\w.>])(?:&\s*)?(?<![\w.>&])(" + "|".join(sorted(map(re.escape, names), key=len, reverse=True)) + r")\b(?!\s*\()")
    out = []
    pos = 0
    depth = 0
    # only inside function bodies and static initialisers; skip declarations at depth 0 (prototypes
    # have '(' after the name and are not matched anyway)
    return pat.sub(lambda m: "PICO_FP(%s)" % m.group(1), code)


def drop_debug_calls(text):
    """PICODBG_*(...) expands to nothing without PICO_DEBUG, but its arguments carry most of the
    string literals - drop the calls (balanced parentheses, strings respected)"""
    out = []
    i, n = 0, len(text)
    pat = re.compile(r"\bPICODBG_[A-Z_]+\s*\(")
    while i < n:
        m = pat.search(text, i)
        if not m:
            out.append(text[i:])
            break
        # not inside a preprocessor line (the macro definitions themselves stay)
        ls = text.rfind("\n", 0, m.start()) + 1
        if text[ls:m.start()].lstrip().startswith("#"):
            out.append(text[i:m.end()])
            i = m.end()
            continue
        out.append(text[i:m.start()])
        depth, j = 1, m.end()
        while j < n and depth:
            c = text[j]
            if c == '"':
                j += 1
                while j < n and text[j] != '"':
                    j += 2 if text[j] == "\\" else 1
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            j += 1
        i = j
    return "".join(out)


def transform(fname, text, strings, functions=True):
    base = os.path.basename(fname)
    # C++ wrappers of the headers: the unity build is C++ already
    text = re.sub(r'#\s*if\s+defined\(\s*__cplusplus\s*\)\s*extern\s+"C"\s*\{\s*#\s*endif', "", text)
    text = re.sub(r'#\s*ifdef\s+__cplusplus\s*extern\s+"C"\s*\{\s*#\s*endif', "", text)
    text = re.sub(r'#\s*if\s+defined\(\s*__cplusplus\s*\)\s*\}\s*#\s*endif', "", text)
    text = re.sub(r'#\s*ifdef\s+__cplusplus\s*\}\s*#\s*endif', "", text)
    # dead code like '#if 0 / } / #endif' (an editor folding trick) would unbalance the brace count
    text = re.sub(r"(?m)^[ \t]*#[ \t]*if[ \t]+0[ \t]*\n.*?^[ \t]*#[ \t]*endif[^\n]*\n", "\n", text, flags=re.S)
    for f, old, new in EDITS:
        if f == base:
            if text.count(old) != 1:
                sys.exit("edit for %s does not match exactly once: %r (%d)" % (base, old, text.count(old)))
            text = text.replace(old, new)
    for f, rx, rep, want in FLOAT_EDITS:
        if f == base:
            text, n = re.subn(rx, rep, text)
            if n != want:
                sys.exit("float edit for %s matched %d times (expected %d): %s" % (base, n, want, rx))
    if functions:
        text, _n = expand_kb_arrays(text, KB_ARRAYS)
        text = drop_debug_calls(text)
    src = Src(text)
    code = src.code
    for old, new in {**KEYWORDS, **RENAMES.get(base, {})}.items():
        code = re.sub(r"\b%s\b" % re.escape(old), new, code)
    nfun = 0
    if functions:
        code = float_fix(code)
        FUNC_NAMES.update(func_names(code))
        code, nfun = mark_functions(code)
        if WRAP_POINTERS:
            code = wrap_function_pointers(code, FUNC_NAMES - MACRO_NAMES)
    # macros defined in this file: undefine them at the end (several files reuse names)
    defined = []
    for line in src.pps:
        m = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)", line)
        if m:
            defined.append(m.group(1))
            MACRO_NAMES.add(m.group(1))
    body = restore(src, code, strings, functions)
    undef = "".join("#undef %s\n" % d for d in dict.fromkeys(defined))
    return body, nfun, undef


def patch_header(name, text, strings):
    if name == "picopal.h":
        # the engine's double type becomes float: no libgcc double helpers in a plugin
        n = text.count("typedef double          picopal_double;")
        if n != 1:
            sys.exit("picopal.h: typedef double picopal_double not found")
        text = text.replace("typedef double          picopal_double;", "typedef float           picopal_double;")
    if name == "picopltf.h":
        text = re.sub(r"#if defined\(__FreeBSD__\).*?#endif", "#define PICO_ENDIANNESS ENDIANNESS_LITTLE", text, count=1, flags=re.S)
        text = re.sub(r"#if __BYTE_ORDER == __BIG_ENDIAN.*?#endif", "", text, count=1, flags=re.S)
    text = re.sub(r'#\s*if\s+defined\(\s*__cplusplus\s*\)\s*extern\s+"C"\s*\{\s*#\s*endif', "", text)
    text = re.sub(r'#\s*ifdef\s+__cplusplus\s*extern\s+"C"\s*\{\s*#\s*endif', "", text)
    text = re.sub(r'#\s*if\s+defined\(\s*__cplusplus\s*\)\s*\}\s*#\s*endif', "", text)
    text = re.sub(r'#\s*ifdef\s+__cplusplus\s*\}\s*#\s*endif', "", text)
    body, _, _ = transform(name, text, strings, functions=False)
    return body


def main():
    global KB_ARRAYS
    KB_ARRAYS = kb_arrays(PICO)
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="only report whether the outputs are up to date")
    a = ap.parse_args()

    os.makedirs(OUT_INC, exist_ok=True)
    os.makedirs(OUT_SRC, exist_ok=True)
    strings = Strings()
    outputs = {}

    for h in sorted(glob.glob(os.path.join(PICO, "lib", "*.h"))):
        n = os.path.basename(h)
        outputs[os.path.join(OUT_INC, n)] = patch_header(n, read(h), strings)

    files = [f for f in sorted(glob.glob(os.path.join(PICO, "lib", "*.c"))) if os.path.basename(f) not in SKIP]
    parts = []
    nfun_total = 0
    # picorsrc.c first (esp_picorsrc.c includes it), then the rest
    ordered = [os.path.join(PICO, "lib", "picorsrc.c")] + files + [os.path.join(PICO, "esp_picorsrc.c")]
    global WRAP_POINTERS
    # pass 1 collects every function name (a pointer may be taken in another file than the definition),
    # pass 2 wraps the uses and writes the result
    for second in (False, True):
        WRAP_POINTERS = second
        strings = Strings()
        parts = []
        nfun_total = 0
        for f in ordered:
            text = read(f)
            if os.path.basename(f) == "esp_picorsrc.c":
                text = text.replace('#include "lib/picorsrc.c"', "")
            body, nfun, undef = transform(f, text, strings)
            nfun_total += nfun
            parts.append("\n// ===== %s (%d functions) =====\n%s\n%s" % (os.path.basename(f), nfun, body, undef))

    header = ("// GENERATED by tools/gen_picotts.py from lib/libesp32_div/pico - do not edit, re-run the tool.\n"
              "// One translation unit: SVOX Pico engine as a BinPlugin (see xblib_04_picotts.cpp).\n")
    outputs[os.path.join(OUT_SRC, "picotts_engine_c.h")] = header + "".join(parts)

    # string blob header
    blob = b"".join(s + b"\0" for s in strings.items)
    pad = (-len(blob)) % 4
    blob += b"\0" * pad
    offs, p = [], 0
    for s in strings.items:
        offs.append(p)
        p += len(s) + 1
    words = [int.from_bytes(blob[i:i + 4], "little") for i in range(0, len(blob), 4)]
    sh = ["// GENERATED by tools/gen_picotts.py - do not edit.",
          "// Every string literal of the amalgamated engine. A plugin has no .rodata of its own, so the",
          "// texts live in this blob, are copied word by word into the MODULE_MEMORY at pFUNC_INIT and",
          "// PICO_S(n) points into that copy.",
          "#ifndef PICOTTS_PLUGIN_STRINGS_H", "#define PICOTTS_PLUGIN_STRINGS_H", "",
          "#define PICO_STR_COUNT %d" % len(strings.items),
          "#define PICO_STR_WORDS %d   // %d bytes" % (len(words), len(blob)), "",
          "static const uint16_t pico_str_off[PICO_STR_COUNT + 1] PROGMEM = {",
          ]
    # offsets as an enum-free macro table so no PROGMEM array has to be read at run time
    sh = sh[:-2]
    sh.append("#define PICO_SOFF(n) (pico_soff_##n)")
    for i, o in enumerate(offs):
        sh.append("#define pico_soff_%d %d" % (i, o))
    sh.append("")
    sh.append("#define PICO_STR_BLOB_INIT \\")
    sh.append(",\\\n".join("  0x%08x" % w for w in words))
    sh.append("")
    sh.append("#endif")
    outputs[os.path.join(OUT_INC, "picotts_plugin_strings.h")] = "\n".join(sh) + "\n"

    stale = []
    for path, text in outputs.items():
        old = read(path) if os.path.exists(path) else None
        if old != text:
            stale.append(path)
            if not a.check:
                with open(path, "w", encoding="utf-8") as f:
                    f.write(text)
    print("%d engine files, %d function definitions marked, %d distinct strings (%d bytes), %d outputs %s"
          % (len(ordered), nfun_total, len(strings.items), len(blob), len(outputs),
             "stale" if a.check and stale else "written" if stale else "unchanged"))
    if a.check and stale:
        sys.exit(1)


if __name__ == "__main__":
    main()
