Modules
=======
modules are relocatable binary drivers for tasmota (currently only ESP8266 supported)
they may be linked and unlinked during runtime (no reboot needed)
in theory any tasmota driver (light, energy, sensor or drv) may be
converted to relocatable format.
however there are limitations. no c++, only c allowed.
no initialized variables, must initialize all variables in code
all variables must be in one structure. all system calls must be vectorized
(vector table with many calls already available)
most annoying thing however is to avoid intrinsic compiler functions.
e.g. floating point math generates intrinsic calls.
therefore several float math functions are provided to circumvent intrinsic calls.
e.g. you may not write  a = b / c  with float variables.
you must use a = fdiv(b, c)


how to create relocatable modules:

you must use a 4M esp8266 device with 2Mb filesystem as development system.
replace the linker file local.eagle.app.v6.common.ld by the one provided in module dir
( the file is automatically generated on clean and has to be replaced again after clean)


1. copy the .ino file you want to convert to the modules directory and rename to .cpp
2. modify the source according to the sample files.
3. add calls not yet in the vector table.
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

7. enable these defines and only one module
#define USE_MODULES
#define EXECUTE_FROM_BINARY

//#define USE_MLX90614_MOD
//#define USE_ADS1115_MOD
//#define USE_SHT3X_MOD
//#define USE_HTU_MOD
#define USE_MP3_PLAYER_MOD

8. test driver functionality
if all is working as expected the resulting relocatable driver is stored
in file system as module.bin. it now can be uploaded to pc and renamed

9. uncomment these defines and recompile
#define EXECUTE_FROM_BINARY
#define USE_MP3_PLAYER_MOD

10. now link the new module and test the relocatable version.

since we can not use the Pin assigments from Tasmota in all cases
we must provide a pin select command for some drivers

modules may be loaded (linked) via file system or via upload
in console "upload module"

module driver commands:

link /module.bin
links a module from filesystem to the next free memory slot. or upload via web ui

unlink X
unlinks (deletes) the module Nr x from system

iniz X
initializes the module Nr x (attaches it to Tasmota)

deiniz X
deinitializes the module Nr x (detaches it from Tasmota and frees memory)

mdir
show a directory of linked modules

dump X
shows a memory dump of module Nr x
