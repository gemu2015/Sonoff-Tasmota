// ============================================================================
// picotts_plugin_pal.h - platform layer of the PicoTTS BinPlugin
// ============================================================================
// Replaces lib/libesp32_div/pico/lib/picopal.c, which talks to libc (string functions, vsprintf,
// stdio, clock, <math.h>). A BinPlugin may not call libc/libgcc directly: those symbols resolve to
// addresses of the plugin-HOST firmware that mean nothing in the running firmware (blib_audit:
// CALL-OUT / ROM). Everything here is plain integer code, or goes through the framework jump
// table (jt[39] fdiv, jt[205] sinf, jt[206] cosf, jt[208] sqrtf, jt[215] expf, jt[73] millis).
//
// `gettbl()->jt` is the same trick as matter/include/mtrc_plugin_libc.h: it gives a file scope jump
// table pointer, so no function needs its own SETREGS.
//
// Copyright (C) 2026  Gerhard Mutz / claude  -  GPL v3 (Tasmota's license)
// ============================================================================
#ifndef PICOTTS_PLUGIN_PAL_H
#define PICOTTS_PLUGIN_PAL_H

#define PICO_JT (gettbl()->jt)

// float helpers through the jump table (picopal_double is float in the plugin build)
#define PICO_FDIV(A, B)  ((( float (*)(float, float) ) PICO_JT[39])((A), (B)))
#define PICO_SINF(A)     ((( float (*)(float) )        PICO_JT[205])((A)))
#define PICO_COSF(A)     ((( float (*)(float) )        PICO_JT[206])((A)))
#define PICO_SQRTF(A)    ((( float (*)(float) )        PICO_JT[208])((A)))
#define PICO_EXPF(A)     ((( float (*)(float) )        PICO_JT[215])((A)))
#define PICO_FABSF(A)    ((A) < 0.0f ? -(A) : (A))

// ---- string functions ----------------------------------------------------------------------
MODULE_PART picopal_int32 picopal_atoi(const picopal_char *s) {
  int32_t sign = 1, v = 0;
  while (*s == ' ' || *s == '\t') { s++; }
  if (*s == '-') { sign = -1; s++; } else if (*s == '+') { s++; }
  while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
  return (picopal_int32)(sign * v);
}

MODULE_PART picopal_int32 picopal_strcmp(const picopal_char *a, const picopal_char *b) {
  while (*a && *a == *b) { a++; b++; }
  return (picopal_int32)*a - (picopal_int32)*b;
}

MODULE_PART picopal_int32 picopal_strncmp(const picopal_char *a, const picopal_char *b, picopal_objsize_t siz) {
  while (siz && *a && *a == *b) { a++; b++; siz--; }
  return siz ? (picopal_int32)*a - (picopal_int32)*b : 0;
}

MODULE_PART picopal_objsize_t picopal_strlen(const picopal_char *s) {
  picopal_objsize_t n = 0;
  while (s[n]) { n++; }
  return n;
}

MODULE_PART picopal_char *picopal_strchr(const picopal_char *s, picopal_char c) {
  for (;; s++) {
    if (*s == c) { return (picopal_char *)s; }
    if (!*s) { return NULL; }
  }
}

MODULE_PART picopal_char *picopal_strstr(const picopal_char *s, const picopal_char *sub) {
  if (!*sub) { return (picopal_char *)s; }
  for (; *s; s++) {
    const picopal_char *p = s, *q = sub;
    while (*p && *q && *p == *q) { p++; q++; }
    if (!*q) { return (picopal_char *)s; }
  }
  return NULL;
}

MODULE_PART picopal_char *picopal_strcpy(picopal_char *d, const picopal_char *s) {
  picopal_char *r = d;
  while ((*d++ = *s++)) { }
  return r;
}

MODULE_PART picopal_char *picopal_strcat(picopal_char *dest, const picopal_char *src) {
  picopal_char *d = dest;
  while (*d) { d++; }
  while ((*d++ = *src++)) { }
  return dest;
}

MODULE_PART picopal_objsize_t picopal_strlcpy(picopal_char *dst, const picopal_char *src, picopal_objsize_t siz) {
  picopal_char *d = dst;
  const picopal_char *s = src;
  picopal_objsize_t n = siz;
  if (n != 0) {
    while (--n != 0) {
      if ((*(d++) = *(s++)) == NULLC) { break; }
    }
  }
  if (n == 0) {
    if (siz != 0) { *d = NULLC; }
    while (*(s++)) { }
  }
  return (picopal_objsize_t)(s - src - 1);
}

// ---- formatted output (only %i %c %s are used by the engine) --------------------------------
MODULE_PART static picopal_char *pico_itoa(picopal_char *end, int32_t v) {
  // writes backwards from `end` (which points at the terminating NUL slot), returns the first char
  uint32_t u = v < 0 ? (0u - (uint32_t)v) : (uint32_t)v;
  *end = 0;
  do { *--end = (picopal_char)('0' + (u % 10)); u /= 10; } while (u);
  if (v < 0) { *--end = '-'; }
  return end;
}

MODULE_PART picopal_objsize_t picopal_vslprintf(picopal_char *dst, picopal_objsize_t siz, const picopal_char *fmt, va_list args) {
  picopal_char buf[16];
  picopal_char *d = dst;
  const picopal_char *f = fmt;
  picopal_char *b;
  picopal_objsize_t len, nnew, n = siz;
  picopal_objsize_t i = 0;
  picopal_char empty[1];

  empty[0] = 0;                              // not "": a literal would live in host .rodata
  if (!f) { f = empty; }
  while (*f) {
    if (*f == '%') {
      switch (*(++f)) {
        case 'i':
        case 'd':
          f++;
          b = pico_itoa(buf + sizeof(buf) - 1, va_arg(args, int));
          break;
        case 'c':
          f++;
          buf[0] = (picopal_char)va_arg(args, int);
          buf[1] = 0;
          b = buf;
          break;
        case 's':
          f++;
          b = (picopal_char *)va_arg(args, char *);
          break;
        default:
          if (n > 0) { (*d++) = '%'; n--; }
          i++;
          b = NULL;
          break;
      }
      if (b) {
        len = picopal_strlcpy(d, b, n);
        i += len;
        nnew = (n > len) ? n - len : 0;
        d += (n - nnew);
        n = nnew;
      }
    } else {
      if (n) { (*d++) = (*f); n--; }
      i++;
      f++;
    }
  }
  if (siz && n) { *d = 0; } else if (siz) { dst[siz - 1] = 0; }
  return i;
}

MODULE_PART picopal_objsize_t picopal_slprintf(picopal_char *dst, picopal_objsize_t siz, const picopal_char *fmt, ...) {
  picopal_objsize_t i;
  va_list args;
  va_start(args, fmt);
  i = picopal_vslprintf(dst, siz, fmt, args);
  va_end(args);
  return i;
}

MODULE_PART picopal_int16 picopal_sprintf(picopal_char *dst, const picopal_char *fmt, ...) {
  picopal_int16 i;
  va_list args;
  va_start(args, fmt);
  i = (picopal_int16)picopal_vslprintf(dst, 0x7fff, fmt, args);
  va_end(args);
  return i;
}

// ---- memory ---------------------------------------------------------------------------------
MODULE_PART void *picopal_mem_copy(const void *src, void *dst, picopal_objsize_t length) {
  const uint8_t *s = (const uint8_t *)src;
  uint8_t *d = (uint8_t *)dst;
  if (d < s || d >= s + length) {
    while (length--) { *d++ = *s++; }
  } else {                                   // overlapping, copy backwards
    d += length; s += length;
    while (length--) { *--d = *--s; }
  }
  return dst;
}

MODULE_PART void *picopal_mem_set(void *dest, picopal_uint8 byte_val, picopal_objsize_t length) {
  uint8_t *d = (uint8_t *)dest;
  while (length--) { *d++ = byte_val; }
  return dest;
}

// ---- math (picopal_double is float here) ----------------------------------------------------
MODULE_PART picopal_double picopal_cos(const picopal_double a)  { return PICO_COSF(a); }
MODULE_PART picopal_double picopal_sin(const picopal_double a)  { return PICO_SINF(a); }
MODULE_PART picopal_double picopal_fabs(const picopal_double a) { return PICO_FABSF(a); }
MODULE_PART picopal_double picopal_quick_exp(const picopal_double y) { return PICO_EXPF(y); }

// the engine's own exp hook (esp_picotts.c defines it for the firmware build)
MODULE_PART picoos_double picoos_quick_exp(const picoos_double y) { return PICO_EXPF(y); }

// ---- file access: the plugin never opens files (voices arrive as memory blocks) --------------
MODULE_PART picopal_char picopal_eol(void) { return '\n'; }
MODULE_PART picopal_File picopal_fopen(picopal_char fileName[], picopal_access_mode mode) { return (picopal_File)0; }
MODULE_PART picopal_File picopal_get_fnil(void) { return (picopal_File)0; }
MODULE_PART picopal_int8 picopal_is_fnil(picopal_File f) { return f == (picopal_File)0; }
MODULE_PART pico_status_t picopal_fflush(picopal_File f) { return PICO_ERR_OTHER; }
MODULE_PART pico_status_t picopal_fclose(picopal_File f) { return PICO_ERR_OTHER; }
MODULE_PART picopal_uint32 picopal_flength(picopal_File f) { return 0; }
MODULE_PART picopal_uint8 picopal_feof(picopal_File f) { return 1; }
MODULE_PART pico_status_t picopal_fseek(picopal_File f, picopal_uint32 offset, picopal_int8 seekmode) { return PICO_ERR_OTHER; }
MODULE_PART pico_status_t picopal_fget_char(picopal_File f, picopal_char *ch) { return PICO_ERR_OTHER; }
MODULE_PART picopal_objsize_t picopal_fread_bytes(picopal_File f, void *ptr, picopal_objsize_t objsize, picopal_uint32 nobj) { return 0; }
MODULE_PART picopal_objsize_t picopal_fwrite_bytes(picopal_File f, void *ptr, picopal_objsize_t objsize, picopal_uint32 nobj) { return 0; }

// ---- debug helpers and timer (unused in the plugin) -----------------------------------------
MODULE_PART void *picopal_mpr_alloc(picopal_objsize_t size) { return NULL; }
MODULE_PART void picopal_mpr_free(void **p) { *p = NULL; }
MODULE_PART pico_status_t picopal_mpr_protect(void *addr, picopal_objsize_t len, picopal_int16 prot) { return PICO_OK; }
MODULE_PART void picopal_get_timer(picopal_uint32 *sec, picopal_uint32 *usec) {
  uint32_t ms = (( uint32_t (*)(void) ) PICO_JT[73])();
  *sec = ms / 1000;
  *usec = (ms % 1000) * 1000;
}

#endif  // PICOTTS_PLUGIN_PAL_H
