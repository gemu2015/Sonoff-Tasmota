// ============================================================================
// mtrc_tables.h — const tables, the way every non-plugin build sees them
// ============================================================================
//
// The matter sources declare their const tables through these macros so the
// plugin build (mtrc_plugin_statics.h, which redefines them) can keep them
// out of the host's .rodata:
//
//   MTRC_FTABLE(T, name, dims) = {...};      file-scope byte table (static)
//   MTRC_FTABLE_X(T, name, dims) = {...};    same, external linkage
//        plugin: PROGMEM, copied once into the heap block at pFUNC_INIT;
//        `name` then refers to the RAM copy
//   MTRC_BTABLE(T, name, dims) = {...};      function-local byte table
//   MTRC_BTABLE_LOAD(T, name, dims);         ... and its use
//        plugin: PROGMEM + a stack copy made where _LOAD stands
//   MTRC_WTABLE(T, name) = {...};            function-local 32-bit table
//   MTRC_WTABLE_PTR(T, name);                ... and its use
//   MTRC_WTABLE_N(name)                      element count
//        plugin: stays in the module; word reads are fine on the instruction
//        bus, only the address needs the load offset
//
// Everywhere else they are plain `static const` arrays.
// ============================================================================
#ifndef MTRC_TABLES_H
#define MTRC_TABLES_H

#ifndef MTRC_PLUGIN_BUILD
#define MTRC_FTABLE(T, name, dims)        static const T name dims
#define MTRC_FTABLE_X(T, name, dims)      const T name dims
#define MTRC_BTABLE(T, name, dims)        static const T name dims
#define MTRC_BTABLE_LOAD(T, name, dims)   do { } while (0)
#define MTRC_WTABLE(T, name)              static const T name[]
#define MTRC_WTABLE_PTR(T, name)          do { } while (0)
#define MTRC_WTABLE_N(name)               (sizeof(name) / sizeof((name)[0]))
#endif

#endif // MTRC_TABLES_H
