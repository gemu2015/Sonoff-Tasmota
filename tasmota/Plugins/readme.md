Plugins
=======
plugins are relocatable binary drivers for tasmota (currently only ESP8266 supported)
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


how to create relocatable plugins:

currentl yonly esp8266 is supported
replace the linker file local.eagle.app.v6.common.ld by the one provided in plugins dir
( the file is automatically generated and must be checked on new espressif versions)
  the module section must be inserted as shown below after *(.ver_number)
    _irom0_text_start = ABSOLUTE(.);
    *(.ver_number)
	/* start modules */
	*(.text.mod_desc)
	*(.text.mod_string)
	*(.text.mod_*)
	*(.text.mod_part)
	*(.text.mod_end)
	/* end modules */

move grepmodule-firmware.py to folder Tools
and place this into your platform_override.ini:
extra_scripts           = ${esp_defaults.extra_scripts}
                          Tools/grepmodule-firmware.py


1. copy the .ino file you want to convert to the plugins directory and rename to .cpp
2. modify the source according to the sample files.
3. add calls not yet in the vector table. in header and xdrv121
4. enable generation of assembly listings and examine the assembly files.
add these to build_flags:
-save-temps=obj
-fverbose-asm
5. no other section may appear then
.text.mod_desc
.text.mod_string
.text.mod_part
.text.mod_end
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
unlinks (deletes) the plugin Nr x from system

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

