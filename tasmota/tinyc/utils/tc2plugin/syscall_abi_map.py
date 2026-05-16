# =====================================================================
# syscall_abi_map.py — TinyC builtin  ->  fixed plugin ABI equivalence
#
# The xdrv_123 jump table (jt[]) is FIXED and never renumbered;
# tasmota/Plugins/module_defines.h aliases plain firmware names to the
# right jt[] slot in plugin mode (and they ARE the real functions in
# native mode). So a dual-format driver just calls the plain name and
# it works in both modes — that is the whole point of this table.
#
# Each TinyC builtin that has a genuine ABI counterpart is mapped to
# the plain firmware name to emit (verified present in module_defines.h).
# `args` rewriting is given where the TinyC signature differs from the
# firmware one.
#
# Builtins that are TinyC *VM constructs* (WebUI widgets, persist/watch
# bookkeeping, the SML integration, the TinyC display layer, TinyC's own
# string/file helpers) have NO jump-table function — they only exist
# inside the VM. Those are listed in VM_INTERNAL so the translator emits
# an honest "reimplement manually" note instead of a misleading TODO.
#
# This is a curated first table covering the high-frequency driver /
# hardware / response / math / libc subset (the part that makes real
# sensor drivers like sht31 translate to callable code). Extend freely.
# =====================================================================

# tinyc_builtin -> firmware plain name (same args unless 'rewrite' given)
# kind: 'fn'  = emit  NAME(args)
#       'tpl' = emit  template.format(a=args-list)
ABI = {
    # --- logging / response / web content -------------------------
    'addLog':         {'emit': 'AddLog'},                # (LL, fmt, …)
    'responseCmnd':   {'emit': 'ResponseCmndChar'},      # (char*)
    'responseAppend': {'emit': 'ResponseAppend_P'},      # (fmt, …)
    'webSend':        {'emit': 'WSContentSend_PD'},      # (fmt, …)
    'webSendP':       {'emit': 'WSContentSend_PD'},
    # --- console / command ----------------------------------------
    'tasmCmd':        {'emit': 'ExecuteCommandReentrant',
                       'note': 'arg shape differs — verify (cmd, SRC_*)'},
    # --- timing ---------------------------------------------------
    'delay':          {'emit': 'delay'},
    'delayMicroseconds': {'emit': 'delayMicroseconds'},
    'millis':         {'emit': 'millis'},
    'micros':         {'emit': 'micros'},
    # --- GPIO -----------------------------------------------------
    'pinMode':        {'emit': 'pinMode'},
    'digitalWrite':   {'emit': 'digitalWrite'},
    'digitalRead':    {'emit': 'digitalRead'},
    'analogRead':     {'emit': 'analogRead'},
    # --- I2C (the fixed jt[] I2C family) --------------------------
    # ABI signatures (module_defines.h): jI2cSetDevice(ADDR,BUS),
    # jI2cWrite8(ADDR,REG,VAL) [NO bus], jI2cRead8(ADDR,REG) [1 byte],
    # jI2cWrite16(ADDR,REG,VAL,BUS), jI2cRead16(ADDR,REG,BUS),
    # jI2cResetActive(REG,BUS), jI2cSetActiveFound(A,B,C).
    # 'args' = lambda adapting the TinyC arg list to the ABI arity.
    # Use the I2C_* abstraction (dual_format_compat.h) — it binds the
    # per-instance wire (mem->xWire / _dual_wire) and works in BOTH
    # native and plugin (plugin routes via SWI2C). Discovery helpers
    # are direct macros; reg-write / byte-read / buffer-read need the
    # I2C_SETWIRE(bus)+raw sequence so they go through generated
    # tc_i2c_* helper shims (see Emitter — same pattern the working
    # hand-written dual drivers inline).
    # discovery probes need I2C_SETWIRE(bus) FIRST (creates the wire) —
    # so they too go through helpers, not bare macros (missing SETWIRE
    # was the LoadStoreError crash on real HW).
    'i2cSetDevice':       {'helper': 'setdev'},  # SETWIRE once/bus here
    'i2cSetActiveFound':  {'emit': 'I2C_SetActiveFound'},  # post-setdev
    'I2cResetActive':     {'emit': 'I2C_ResetActive'},     # post-setdev
    'i2cActive':          {'emit': 'I2cActive'},
    'i2cWrite8':  {'helper': 'w8'},     # tc_i2c_w8(a,r,v,bus) -> bool ok
    'i2cRead8':   {'helper': 'r8'},     # tc_i2c_r8(a,r,bus)   -> int byte
    'i2cRead0':   {'helper': 'rbuf'},   # tc_i2c_rbuf(a,buf,n,bus) -> bool
    'i2cRead':    {'helper': 'rbuf'},
    'i2cWrite16': {'emit': 'I2C_Write16'},                   # macro, 4
    'i2cRead16':  {'emit': 'I2C_Read16'},                    # macro, 3
    'i2cValidRead16': {'emit': 'I2C_ValidRead16'},           # macro, 4
    'i2cWrite':   {'manual': 'raw arbitrary-length I2C write — inline '
                   'I2C_beginTransmission/I2C_write/I2C_endTransmission '
                   'by hand (variable payload, no generic shim)'},
    # --- raw Wire (jt[12..16]) ------------------------------------
    'wireBegin':       {'emit': 'beginTransmission'},
    'wireWrite':       {'emit': 'write'},
    'wireEnd':         {'emit': 'endTransmission'},
    'wireRequest':     {'emit': 'requestFrom'},
    'wireRead':        {'emit': 'read'},
    # --- string / libc (jstr* family) -----------------------------
    'strlen':  {'emit': 'strlen'},
    'strcpy':  {'emit': 'strcpy'},
    'strncpy': {'emit': 'strncpy'},
    'strcat':  {'emit': 'strcat'},
    'strcmp':  {'emit': 'strcmp'},
    'strchr':  {'emit': 'strchr'},
    'atoi':    {'emit': 'atoi'},
    'atof':    {'emit': 'CharToFloat',
               'note': 'TinyC atof -> Tasmota CharToFloat helper'},
    'strlcpy': {'emit': 'strlcpy'},
    'sprintf': {'emit': 'snprintf_P', 'rewrite': 'sprintf'},  # buf,fmt,..
    # --- text tables ----------------------------------------------
    # TinyC LGetString(index,dst) is a FIXED 21-entry localized-label
    # table baked into the VM (D_TEMPERATURE..D_ABSOLUTE_HUMIDITY). The
    # working dual drivers use GetTextIndexed(dst,sizeof(dst),idx,
    # GSTR(table)) with a file-scope PROGMEM '|' table. So we emit the
    # SAME D_* table and map to that exact pattern (Emitter handles it).
    'LGetString':     {'lstr': True},
    'getTextIndexed': {'lstr': True},
    # --- math (only these exist in the jt[]) — all return float ---
    'sin':  {'emit': 'sinf', 'ret': 'f'},
    'cos':  {'emit': 'cosf', 'ret': 'f'},
    'log':  {'emit': 'logf', 'ret': 'f'},
    'exp':  {'emit': 'expf', 'ret': 'f'},   # jt[215] — added to jumptable
    'sqrt': {'emit': 'sqrtf', 'ret': 'f'},
    'pow':  {'emit': 'FastPrecisePowf', 'ret': 'f'},
    'abs':  {'emit': 'abs', 'ret': 'i'},
    'isnan':{'emit': 'isnan', 'ret': 'i'},
    # --- sensor display helpers (jt[]) — float in/out -------------
    'convertTemp':     {'emit': 'ConvertTemp', 'ret': 'f'},
    'convertHumidity': {'emit': 'ConvertHumidity', 'ret': 'f'},
}

# Soft-float jt[] helpers (present in BOTH dual_format_compat.h native
# defs AND module_defines.h plugin jt[] aliases). Emitting these instead
# of `+ - * / < > ==` on floats is what stops the C compiler generating
# __addsf3/__mulsf3/__floatsisf… intrinsics the plugin loader can't bind.
SOFTFLOAT = {
    'add': 'fadd', 'sub': 'fdiff', 'mul': 'fmul', 'div': 'fdiv',
    'lt': 'ltsf2', 'gt': 'gtsf2', 'eq': 'eqsf2',
    'i2f': 'tofloat',          # int  -> float
    'u2f': 'floatunsisf',      # uint -> float
    'f2u': 'fixunssfsi',       # float -> uint  (no signed fixsfsi in jt[])
}

# TinyC VM constructs with NO jump-table function. Matched by exact
# name or by prefix (the value is just documentation of the family).
VM_INTERNAL_PREFIX = {
    'web':  'TinyC WebUI widget system (webButton/Slider/Checkbox/'
            'Number/Pulldown/PageLabel/Chart) — no plugin-ABI widget '
            'layer; reimplement with WSContentSend_PD HTML',
    'dsp':  'TinyC display abstraction (dspText/Pos/Color/Line…) — '
            'use the renderer/uDisplay API directly in a plugin',
    'sml':  'TinyC SML integration (smlGet/smlScripterLoad…) — bind '
            'to the xsns_53 SML API; no single jt[] entry',
}
VM_INTERNAL_NAMES = {
    # persist / watch bookkeeping (VM-only)
    'saveVars', 'snapshot', 'changed', 'written', 'persist',
    # TinyC filesystem wrapper (use LittleFS/UFS directly in a plugin)
    'fileOpen', 'fileClose', 'fileRead', 'fileWrite', 'fileSize',
    'fileSeek', 'fileDelete', 'fileExists',
    # TinyC string helpers (no 1:1 libc/jt — reimplement)
    'strFind', 'strToken', 'strSub', 'strReplace', 'strSplit',
    # TinyC task / cross-VM share machinery
    'spawnTask', 'killTask', 'taskRunning',
    'shareSetInt', 'shareGetInt', 'shareSetFloat', 'shareGetFloat',
    'shareSetStr', 'shareGetStr', 'shareHas', 'shareDelete',
    'shareDump', 'dumpPersist',
    # TinyC print helpers route to a debug sink, not a driver fn
    'print', 'printStr', 'printString',
}


def lookup(name):
    """('abi',info) | ('manual',reason) | ('vm',reason) | (None,None)."""
    if name in ABI:
        info = ABI[name]
        if 'manual' in info:
            return 'manual', info['manual']
        if 'helper' in info:
            return 'helper', info['helper']
        if 'lstr' in info:
            return 'lstr', info['lstr']
        return 'abi', info
    if name in VM_INTERNAL_NAMES:
        return 'vm', 'TinyC VM construct — no jump-table equivalent'
    for pfx, why in VM_INTERNAL_PREFIX.items():
        if name.startswith(pfx):
            return 'vm', why
    return None, None
