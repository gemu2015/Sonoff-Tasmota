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

core3path = "framework-arduinoespressif32/tools/esp32-arduino-libs/esp32/ld/sections.ld"

if mcu == "esp8266":
        libpath = platform.get_package_dir("framework-arduinoespressif8266")+"/tools/sdk/ld/eagle.app.v6.common.ld.h"
        match = "*(.ver_number)"
        mlen = 0
if mcu == "esp32":
        libpath = platform.get_package_dir("framework-arduinoespressif32")+"/tools/esp32-arduino-libs/esp32/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)

if mcu == "esp32s2":
        libpath = platform.get_package_dir("framework-arduinoespressif32")+"/tools/esp32-arduino-libs/esp32s2/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)
if mcu == "esp32s3":
        # has 4 variants (opi_opi,opi_qspi,qio_opi,qio_qspi, )
        memory_type = env.BoardConfig().get("build.arduino.memory_type", "qio_qspi")
        libpath = platform.get_package_dir("framework-arduinoespressif32")+"/tools/esp32-arduino-libs/esp32s3/"+memory_type+"/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)
if mcu == "esp32c3":
        libpath = platform.get_package_dir("framework-arduinoespressif32")+"/tools/esp32-arduino-libs/esp32c3/ld/sections.ld"
        match = '+= _esp_flash_mmap_prefetch_pad_size;'
        mlen = len(match)

# idf5 has unique linker path
filok = os.path.isfile(libpath)
if filok == False :
        print("take default path")
        libpath = platform.get_package_dir("framework-arduinoespressif32")+"/tools/esp32-arduino-libs/esp32/ld/sections.ld"

print("link file path: "+libpath)
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