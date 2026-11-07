// plugin_host_stubs.cpp -- link stubs for the plugin HOST build only (env tasmota32-plugin).
//
// 07.11.2026: the prebuilt libs of Tasmota Arduino core 3.3.12 for the classic ESP32 are built
// with CONFIG_MBEDTLS_SHA512_C off, but the Arduino "Update" library (UpdaterSHA512.cpp) calls
// mbedtls_sha512_*, so the host firmware of build_plugin.py no longer linked ("undefined
// reference to mbedtls_sha512_*", every classic-ESP32 plugin build failed, not only Matter).
//
// The host firmware of this env is never run or flashed: build_plugin.py only cuts the module
// out of the linked ELF. So weak do-nothing definitions are enough to close the link. They are
// WEAK, a real implementation (a core whose libs have SHA-512) wins, and they exist only with
// -Dplugin_host_build, so no normal firmware gets them.
// NOT a general "ignore unresolved symbols": an unresolved call from a MODULE must still fail.
#ifdef plugin_host_build
extern "C" {
__attribute__((weak)) void mbedtls_sha512_init(void *ctx)                          { (void)ctx; }
__attribute__((weak)) void mbedtls_sha512_free(void *ctx)                          { (void)ctx; }
__attribute__((weak)) int  mbedtls_sha512_starts(void *ctx, int is384)             { (void)ctx; (void)is384; return -1; }
__attribute__((weak)) int  mbedtls_sha512_update(void *ctx, const unsigned char *in, unsigned int n) { (void)ctx; (void)in; (void)n; return -1; }
__attribute__((weak)) int  mbedtls_sha512_finish(void *ctx, unsigned char out[64]) { (void)ctx; (void)out; return -1; }
}
#endif
