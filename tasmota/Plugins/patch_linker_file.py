# insert plugin sections into linker file
Import("env")

import os
import shutil
import pathlib
import tasmotapiolib
import binascii
from os.path import join


platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")

print("patching linker file")

if mcu == "esp8266":
        libpath = "/Users/gerhardmutz1/.platformio/packages/framework-arduinoespressif8266/tools/sdk/ld/eagle.app.v6.common.ld.h"
        match = "*(.ver_number)"
        mlen = 0
if mcu == "esp32":
        libpath = "/Users/gerhardmutz1/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)
if mcu == "esp32s2":
        libpath = "/Users/gerhardmutz1/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s2/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)
if mcu == "esp32s3":
        libpath = "/Users/gerhardmutz1/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)
if mcu == "esp32c3":
        libpath = "/Users/gerhardmutz1/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32c3/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)

with open(libpath) as f:
    data = f.read()
    f.close()

    index = data.find("/* start plugins */")
    if index < 0 :
        index = data.find(match)+mlen
        part1 = data[0:index]
        part2 = data[index:]

        insert = ""
        if mcu == "esp8266":
            insert = '/* start plugins */\n \
	        *(.text.mod_desc)\n\
	        *(.text.mod_string)\n\
	        *(.text.mod_*)\n\
	        *(.text.mod_part)\n\
	        *(.text.mod_end)\n\
	        /* end plugins */\n'
        else :
            insert = '/* start plugins */\n \
	        *(.plugin.mod_desc)\n\
	        *(.plugin.mod_string)\n\
	        *(.plugin.mod_part.literal)\n\
	        *(.plugin.mod_part)\n\
	        *(.plugin.mod_end)\n\
	        /* end plugins */\n'
                        

        out = part1 + insert + part2

        with open(libpath, "w") as wf:
            wf.write(out)
            wf.close()

        print("patch complete")
    
    else :
        print("already patched")