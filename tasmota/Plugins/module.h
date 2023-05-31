#ifndef _MODULE_H_
#define _MODULE_H_

#include <stdio.h>
#include <stddef.h>
#include <Wire.h>
#include <Stream.h>
#include <HardwareSerial.h>

#ifdef USE_BINPLUGINS

#define AGPIO(x) ((x)<<5)
#define BGPIO(x) ((x)>>5)

#ifndef SerConfu8
#define SerConfu8 uint8_t
#endif
//#include "tasmota_compat.h"
#include "../include/tasmota.h"
#include "i18n.h"
#include "../include/tasmota_globals.h"


#ifndef D_SENSOR_NONE
#include "../language/de_DE.h"
#endif

#include "../include/tasmota_template.h"

#include "../include/tasmota_types.h"

extern TSettings* Settings;

#define SETTINGS TSettings

#ifndef PROGMEM
#define PROGMEM
#endif

#include "modules_def.h"


/* linker sections
*(.text.mod_desc)
*(.text.mod_string)
*(.text.mod*)
*(.text.mod_end)
*/


//static int32_t mod_func_execute(MODULES_TABLE *, uint32_t);
static int32_t mod_func_execute(uint32_t);

static void end_of_module(void);

#endif



/*

// *.h file
// ...
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

typedef void* mylibrary_mytype_t;

EXTERNC mylibrary_mytype_t mylibrary_mytype_init();
EXTERNC void mylibrary_mytype_destroy(mylibrary_mytype_t mytype);
EXTERNC void mylibrary_mytype_doit(mylibrary_mytype_t self, int param);

#undef EXTERNC
// ...

// *.cpp file
mylibrary_mytype_t mylibrary_mytype_init() {
  return new MyType;
}

void mylibrary_mytype_destroy(mylibrary_mytype_t untyped_ptr) {
   MyType* typed_ptr = static_cast<MyType*>(untyped_ptr);
   delete typed_ptr;
}

void mylibrary_mytype_doit(mylibrary_mytype_t untyped_self, int param) {
   MyType* typed_self = static_cast<MyType*>(untyped_self);
   typed_self->doIt(param);
}

*/

#endif // _MODULE_H_
