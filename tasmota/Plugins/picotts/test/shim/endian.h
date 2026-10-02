/* host test shim: macOS has no <endian.h> */
#ifndef SHIM_ENDIAN_H
#define SHIM_ENDIAN_H
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
