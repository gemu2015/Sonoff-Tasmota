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

typedef struct { matter_ctx_t *mtrc_ctx; } MODULE_MEMORY;  // the plugin's only persistent state
#define MTRC_MEM ((MODULE_MEMORY *)gettbl()->mod_memory)
#define g_ptr    (MTRC_MEM->mtrc_ctx)   // lvalue → both `if(!g_ptr)` and `g_ptr = …` work
#define g        (*g_ptr)

#endif // MTRC_PLUGIN_MEM_H
