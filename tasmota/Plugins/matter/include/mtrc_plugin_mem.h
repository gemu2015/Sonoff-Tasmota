// mtrc_plugin_mem.h — context-pointer access for the matter_c BinPlugin (Fork B).
//
// matter_c stores its whole state behind `static matter_ctx_t *g_ptr` + the macro
// `#define g (*g_ptr)`, used thousands of times. In a relocatable plugin a
// file-scope MUTABLE pointer is forbidden — the de-risk proved it crashes: such a
// symbol lands in the flash-mapped, read-only module image, not in RAM, so the
// `g_ptr = …` write goes nowhere (the same class as `g_cr` → "Software reset CPU").
//
// Fix (gemu's call): keep the ~22 KB context in PSRAM (matter_special_malloc), but
// hold its POINTER in the framework's MODULE_MEMORY (RAM, allocated by ALLOCMEM at
// iniz, freed by RETMEM at deiniz). `g_ptr` and `g` are redefined as macros that
// reach MODULE_MEMORY through gettbl()->mod_memory. Because MTRC_MEM->mtrc_ctx is
// an LVALUE, every existing site — `if (!g_ptr)`, `g_ptr = malloc(...)`, `g.field`
// — compiles UNCHANGED. matter is not a hot path, so the per-access gettbl()+deref
// is fine.
//
// INTEGRATION REQUIREMENT: the firmware must make gettbl() resolve to THIS module
// before calling any matter entry point (matter_init/loop/handle_rx/invoke/web…),
// i.e. set the module register exactly as the normal mod_func_execute dispatch
// does. The plugin entry .cpp wraps each firmware→matter call to ensure that.
//
// MUST be #included AFTER matter_ctx_t is defined (it is named below) and after
// module_defines.h (gettbl / MODULES_TABLE). Only the PLUGIN copy of matter_c.c
// uses this; the built-in lib keeps the plain `static g_ptr`.
#ifndef MTRC_PLUGIN_MEM_H
#define MTRC_PLUGIN_MEM_H

struct mtrc_crypto_ops;                 // tagged — fwd-decl so we don't depend on include order
typedef struct {
  matter_ctx_t *mtrc_ctx;               // the ~22 KB PSRAM context (g), heap-allocated by matter_init
  const struct mtrc_crypto_ops *cr;     // crypto seam — bound by mtrc_crypto_bind() BEFORE matter_init,
                                        // so it can't live in g (still NULL then); rides MODULE_MEMORY
} MODULE_MEMORY;                         // the plugin's only persistent RAM (ALLOCMEM at iniz)
// `mem` is the SETMEMREGS-bound local every matter function carries (GET_MTBL;
// GET_JT; MODULE_MEMORY *mem = …), so the ctx is resolved ONCE at entry and
// captured before any nested firmware call could flip the module register —
// the idiomatic plugin pattern (vs a per-access gettbl()). lvalues, so both
// `if(!g_ptr)` and `g_ptr = …` / `g_cr = ops` still compile unchanged.
#define g_ptr    (mem->mtrc_ctx)
#define g        (*g_ptr)
#define g_cr     (mem->cr)

#endif // MTRC_PLUGIN_MEM_H
