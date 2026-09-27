/*
  xdrv_124_matter_dispatch.h — TinyC Matter: built-in lib or the MATTERF BinPlugin

  Firmware with USE_MATTER_C and USE_BINPLUGINS carries both: the matter_c lib
  linked in, and the option to load the same code as the MATTERF BLIB
  (tasmota/Plugins/xblib_03_matter_full.cpp). Every matter_* call of the TinyC
  glue (xdrv_124_tinyc.ino) and of the VM's mtr* syscalls goes through the table
  below; the macros at the end redirect the existing calls, so the call sites
  stay as they are.

  The choice is made ONCE, at the first matter_init() (mtrc_ensure_inited):
  a MATTERF module in the plugin partition is initialized if nobody did yet;
  if it then exports the whole API, it is used and gets the
  firmware's BearSSL primitives (mtrc_crypto_bind); otherwise the built-in lib.
  After that it is fixed — the Matter state lives in whichever one was chosen.
  Before the choice every call goes to the built-in lib, as without plugins.
  If the chosen plugin is unloaded later, all calls go to stubs that do
  nothing and report MATTER_ERR_NOT_INIT (a reboot starts over).

  Included by xdrv_124_tinyc.ino right after its first #include "matter_c.h".
*/
#ifndef XDRV_124_MATTER_DISPATCH_H
#define XDRV_124_MATTER_DISPATCH_H

#include "../Plugins/matter/include/mtrc_crypto_ops.h"   // firmware -> plugin crypto seam

extern "C" TC_BLIB_REG_ENTRY *tc_blib_lookup(const char *name);

// R(ret, name, params, args, dead_value) for functions with a result,
// V(name, params, args) for void ones. name = the part after "matter_".
#define MTRC_API_LIST(R, V) \
  R(matter_err_t, init, (const matter_port_t *p, const matter_config_t *c), (p, c), MATTER_ERR_NOT_INIT) \
  V(udp_rx, (const uint8_t *ip6, uint16_t port, const void *buf, size_t len), (ip6, port, buf, len)) \
  R(matter_err_t, start, (void), (), MATTER_ERR_NOT_INIT) \
  V(loop, (void), ()) \
  R(const char *, qr_uri, (void), (), "") \
  R(bool, qr_dark, (int x, int y), (x, y), false) \
  R(int, qr_size, (void), (), 0) \
  R(matter_err_t, factory_reset, (void), (), MATTER_ERR_NOT_INIT) \
  V(set_commissionable, (int on), (on)) \
  R(matter_err_t, open_commissioning_window, (void), (), MATTER_ERR_NOT_INIT) \
  R(const char *, manual_code, (void), (), "") \
  R(matter_err_t, set_label, (uint16_t ep, const char *name), (ep, name), MATTER_ERR_NOT_INIT) \
  R(matter_err_t, set_attr_uint, (uint16_t ep, uint32_t cl, uint32_t at, uint64_t v), (ep, cl, at, v), MATTER_ERR_NOT_INIT) \
  R(matter_err_t, set_attr_scaled, (uint16_t ep, uint32_t cl, uint32_t at, float f, int32_t sc), (ep, cl, at, f, sc), MATTER_ERR_NOT_INIT) \
  V(reset_model, (void), ()) \
  R(matter_err_t, queue_event, (uint16_t ep, uint32_t cl, uint32_t ev, int32_t a, int32_t b), (ep, cl, ev, a, b), MATTER_ERR_NOT_INIT) \
  R(int, get_attr_uint, (uint16_t ep, uint32_t cl, uint32_t at, uint64_t *out), (ep, cl, at, out), 0) \
  R(int, add_endpoint, (uint32_t dt), (dt), -1) \
  R(matter_err_t, add_cluster, (uint16_t ep, uint32_t cl), (ep, cl), MATTER_ERR_NOT_INIT) \
  R(matter_err_t, add_attr, (uint16_t ep, uint32_t cl, uint32_t at, int type, int wr), (ep, cl, at, type, wr), MATTER_ERR_NOT_INIT)

// the table type
#define MTRC_F_R(ret, name, params, args, dv)  ret (*name) params;
#define MTRC_F_V(name, params, args)           void (*name) params;
typedef struct { MTRC_API_LIST(MTRC_F_R, MTRC_F_V) } mtrc_api_t;

// stubs: after the chosen plugin was unloaded, and as the "built-in" side of a
// plugin-only build
#define MTRC_D_R(ret, name, params, args, dv)  static ret mtrc_dead_##name params { return dv; }
#define MTRC_D_V(name, params, args)           static void mtrc_dead_##name params { }
MTRC_API_LIST(MTRC_D_R, MTRC_D_V)
#define MTRC_DT_R(ret, name, params, args, dv) mtrc_dead_##name,
#define MTRC_DT_V(name, params, args)          mtrc_dead_##name,
static const mtrc_api_t mtrc_api_dead = { MTRC_API_LIST(MTRC_DT_R, MTRC_DT_V) };

#ifdef USE_MATTER_C_PLUGIN_ONLY
// Matter only as the plugin (for boards without PSRAM, where the built-in lib's
// ~33 KB of .bss would sit next to the plugin's heap block): nothing references
// the lib, so the linker leaves its code and .bss out. Without the plugin every
// matter_* call answers like an unloaded plugin.
static const mtrc_api_t mtrc_api_builtin = { MTRC_API_LIST(MTRC_DT_R, MTRC_DT_V) };
#else
// built-in lib (the real matter_* — the redirecting macros come further down)
#define MTRC_B_R(ret, name, params, args, dv)  matter_##name,
#define MTRC_B_V(name, params, args)           matter_##name,
static const mtrc_api_t mtrc_api_builtin = { MTRC_API_LIST(MTRC_B_R, MTRC_B_V) };
#endif

static mtrc_api_t        mtrc_api_plugin;          // filled from the BLIB exports
static const mtrc_api_t *mtrc_api_sel = nullptr;   // latched at the first matter_init()

// The table in use. Before the choice: the built-in lib (unchanged behaviour).
static const mtrc_api_t *mtrc_api(void) {
  if (mtrc_api_sel == &mtrc_api_plugin && !tc_blib_lookup("matter_loop")) {
    mtrc_api_sel = &mtrc_api_dead;                 // plugin unloaded under us
    AddLog(LOG_LEVEL_ERROR, PSTR("MTR: Matter plugin unloaded - Matter off until restart"));
  }
  return mtrc_api_sel ? mtrc_api_sel : &mtrc_api_builtin;
}

// Fill the plugin table; true only if the plugin exports the complete API.
static bool mtrc_api_from_plugin(void) {
  TC_BLIB_REG_ENTRY *r;
#define MTRC_L_R(ret, name, params, args, dv) \
  if (!(r = tc_blib_lookup("matter_" #name))) return false; \
  mtrc_api_plugin.name = (ret (*) params)r->fn;
#define MTRC_L_V(name, params, args) \
  if (!(r = tc_blib_lookup("matter_" #name))) return false; \
  mtrc_api_plugin.name = (void (*) params)r->fn;
  MTRC_API_LIST(MTRC_L_R, MTRC_L_V)
  return true;
}

// The plugin gets the firmware's BearSSL primitives by pointer (Fork B, proven
// on .156 with the stage-2 self test: vtables work without EXEC_OFFSET).
static bool mtrc_bind_plugin_crypto(void) {
  TC_BLIB_REG_ENTRY *r = tc_blib_lookup("mtrc_crypto_bind");
  if (!r) return false;
  static mtrc_crypto_ops ops;
  ops.sha256_init   = br_sha256_init;     ops.sha256_update = br_sha256_update;
  ops.sha256_out    = br_sha256_out;
  ops.hmac_key_init = br_hmac_key_init;   ops.hmac_init     = br_hmac_init;
  ops.hmac_update   = br_hmac_update;     ops.hmac_out      = br_hmac_out;
  ops.hkdf_init     = br_hkdf_init;       ops.hkdf_inject   = br_hkdf_inject;
  ops.hkdf_flip     = br_hkdf_flip;       ops.hkdf_produce  = br_hkdf_produce;
  ops.ecdsa_sign_raw= br_ecdsa_i15_sign_raw; ops.ecdsa_vrfy_raw= br_ecdsa_i15_vrfy_raw;
  ops.aes_ct_ctrcbc_init = br_aes_ct_ctrcbc_init;
  ops.ccm_init      = br_ccm_init;        ops.ccm_reset     = br_ccm_reset;
  ops.ccm_aad_inject= br_ccm_aad_inject;  ops.ccm_flip      = br_ccm_flip;
  ops.ccm_run       = br_ccm_run;         ops.ccm_get_tag   = br_ccm_get_tag;
  ops.ccm_check_tag = br_ccm_check_tag;
  ops.ec_p256_m15   = &br_ec_p256_m15;    ops.sha256_vtable = &br_sha256_vtable;
  ((void (*)(const mtrc_crypto_ops *))r->fn)(&ops);
  return true;
}

// A MATTERF module in the plugin partition counts as present even if nobody
// has run `iniz` on it: plugins are not initialized at boot, and a TinyC script
// with autostart reaches matter_init() long before anyone could. Initialize it
// here, then its exports are registered.
static void mtrc_plugin_autoinit(void) {
  if (!plugins.ready) return;
  for (uint32_t i = 0; i < MAX_PLUGINS; i++) {
    if (!modules[i].mod_addr) continue;
    // header word by word: the partition is mapped on the instruction bus,
    // byte reads (strncmp on fm->name) fault on ESP32/S3
    uint32_t hdr[sizeof(FLASH_MODULE) / 4];
    const volatile uint32_t *lp = (const volatile uint32_t *)modules[i].mod_addr;
    for (uint32_t w = 0; w < sizeof(FLASH_MODULE) / 4; w++) { hdr[w] = lp[w]; }
    if (strncmp(((FLASH_MODULE *)hdr)->name, "MATTERF", 16) != 0) continue;
    if (!modules[i].flags.initialized) {
      AddLog(LOG_LEVEL_INFO, PSTR("MTR: initializing the Matter plugin (module %u)"), (unsigned)i + 1);
      Init_module(i);
    }
    return;
  }
}

// matter_init() with the one-time choice in front of it.
static matter_err_t mtrc_select_and_init(const matter_port_t *p, const matter_config_t *c) {
  if (!mtrc_api_sel) {
    mtrc_plugin_autoinit();
    if (mtrc_api_from_plugin() && mtrc_bind_plugin_crypto()) {
      mtrc_api_sel = &mtrc_api_plugin;
      AddLog(LOG_LEVEL_INFO, PSTR("MTR: using the Matter plugin (MATTERF)"));
    } else {
      mtrc_api_sel = &mtrc_api_builtin;
#ifdef USE_MATTER_C_PLUGIN_ONLY
      AddLog(LOG_LEVEL_ERROR, PSTR("MTR: no Matter plugin (MATTERF) - this firmware has no built-in Matter"));
#else
      AddLog(LOG_LEVEL_INFO, PSTR("MTR: using the built-in Matter"));
#endif
    }
  }
  return mtrc_api()->init(p, c);
}

// Redirect every matter_* call below this point.
#define matter_init(p, c)                          mtrc_select_and_init(p, c)
#define matter_udp_rx(...)                         mtrc_api()->udp_rx(__VA_ARGS__)
#define matter_start()                             mtrc_api()->start()
#define matter_loop()                              mtrc_api()->loop()
#define matter_qr_uri()                            mtrc_api()->qr_uri()
#define matter_qr_dark(...)                        mtrc_api()->qr_dark(__VA_ARGS__)
#define matter_qr_size()                           mtrc_api()->qr_size()
#define matter_factory_reset()                     mtrc_api()->factory_reset()
#define matter_set_commissionable(...)             mtrc_api()->set_commissionable(__VA_ARGS__)
#define matter_open_commissioning_window()         mtrc_api()->open_commissioning_window()
#define matter_manual_code()                       mtrc_api()->manual_code()
#define matter_set_label(...)                      mtrc_api()->set_label(__VA_ARGS__)
#define matter_set_attr_uint(...)                  mtrc_api()->set_attr_uint(__VA_ARGS__)
#define matter_set_attr_scaled(...)                mtrc_api()->set_attr_scaled(__VA_ARGS__)
#define matter_reset_model()                       mtrc_api()->reset_model()
#define matter_queue_event(...)                    mtrc_api()->queue_event(__VA_ARGS__)
#define matter_get_attr_uint(...)                  mtrc_api()->get_attr_uint(__VA_ARGS__)
#define matter_add_endpoint(...)                   mtrc_api()->add_endpoint(__VA_ARGS__)
#define matter_add_cluster(...)                    mtrc_api()->add_cluster(__VA_ARGS__)
#define matter_add_attr(...)                       mtrc_api()->add_attr(__VA_ARGS__)

#endif // XDRV_124_MATTER_DISPATCH_H
