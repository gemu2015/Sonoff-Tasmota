Plugins
=======
plugins are relocatable binary drivers for tasmota (now supported for esp8266, esp32, esp32-s2, esp32-s3, esp32-c3 and working also with core3)
they may be linked and unlinked during runtime (no reboot needed)
in theory any tasmota driver (light, energy, sensor or drv) may be
converted to relocatable format.
however there are limitations. no c++, only c allowed.
no initialized variables, must initialize all variables in code
all variables must be in one structure. all system calls must be vectorized
(vector table with many calls already available)
most annoying thing however is to avoid intrinsic compiler functions.
e.g. floating point math generates builtin calls.
therefore several float math functions are provided to circumvent builtin calls.
e.g. you may not write  a = b / c  with float variables.
you must use a = fdiv(b, c)
since RISCV ESPs use special floating point constants from ROM memory
to circumvent this, we have to specify those constants as fractions  with FLCONST(x, y) 
so specify 273.15  as FLCONST(27315, 100)
another problem ist that we would need at least 3 types of binaries multiplied by language variants
the driver handler itself uses about 18,3 kB of flash 

how to create relocatable plugins:

several python scripts help in converting (see instruction in script)
prep_driver.py  converts a tasmota driver to a .cpp file and does already a lot of needed conversion
cpp2c.py converts a c++ class file to pure c (needed for c++ library files)


linker files are automatically patched for all cpu types by this python script: patch_linker_file.py
binaries are automatically extracted from firmware.bin by this python script: grepmodule-firmware.py

esp32 needs an extra partition where binaries are stored (custom)
example:

	# Name,   Type, SubType, Offset,  Size, Flags
	nvs,      data, nvs,     0x9000,  0x5000,
	otadata,  data, ota,     0xe000,  0x2000,
	safeboot, app,  factory, 0x10000, 0xD0000,
	app0,     app,  ota_0,   0xE0000, 0x2C0000,
	custom,   app,  test,    0x3A0000,0x10000,
	spiffs,   data, spiffs,  0x3B0000,0x50000,


add these entries to extra_scripts:

extra_scripts           = ${esp_defaults.extra_scripts}
                        pre:Tasmota/Plugins/patch_linker_file.py
                        post:Tasmota/Plugins/grepmodule-firmware.py
                        post:pio-tools/obj-dump.py

this patches all linker files, so you do not need to do it manually
also an asm file is generated with which you can check for unwanted library calls
and the plugin is extracted from the binary, according to the selected cpu
ending .bin (esp8266) _32.bin (tensilica ESP32) _32r.bin (riscv ESP32)


1. copy the .ino file you want to convert to the plugins directory
2. modify the source according to the sample files.
3. add calls not yet in the vector table. in header and xdrv123
assembly file firmware.asm is created by above scripts

5. no other section may appear then
.text.mod_desc
.text.mod_string
.text.mod_part
.text.mod_end

or for ESP32
.plugin.mod_desc
.plugin.mod_string
.plugin.mod_part.literal
.plugin.mod_part
.plugin.mod_end

6. examine call instructions, no call to external symbol may appear

7. enable these defines and only one plugin 
#define USE_BINPLUGINS
#define EXECUTE_FROM_BINARY

//#define USE_MLX90614_MOD
//#define USE_ADS1115_MOD
#define USE_SHT3X_MOD
//#define USE_HTU_MOD
//#define USE_MP3_PLAYER_MOD


8. test driver functionality
if all is working as expected the resulting relocatable driver is stored
along with your firmware bin file with filename MODULE_NAME.bin

9. uncomment these defines and recompile
#define EXECUTE_FROM_BINARY
#define USE_SHT3X_MOD

10. now link the new plugin via upload and test the relocatable version.

since we can not use the Pin assigments from Tasmota in all cases
we could provide a pin select command for some drivers. 
GUI solution for up to 4 pins is available with permanent save

plugins may be loaded (linked) via file system or via upload
in console "upload plugins"

plugin driver commands:

link /plugin.bin
links a plugin from filesystem to the next free memory slot. or upload via web ui

unlink X
unlinks (deletes) the plugin Nr x from system, 0 deletes all plugins from system

iniz X
initializes the plugin Nr x (attaches it to Tasmota) iniz 0 initializes all drivers

deiniz X
deinitializes the plugin Nr x (detaches it from Tasmota and frees memory)

mdir
show a JSON list of linked plugins

dump X
shows a memory dump of plugin Nr x

after a reboot all plugins persist 
and if option A 7 is set all plugins are initialized too

todoo:

esp8266  	ready, ok, multiple drivers working stable
esp32		ready
			have to disable stack check per function (solved inside macro, "no-stack-protector") 
esp32 risc	ready
			have to disable save, restore epilog lib calls for complete project (-mno-save-restore)

