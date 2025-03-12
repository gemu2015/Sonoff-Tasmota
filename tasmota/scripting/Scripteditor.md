# Script Editor for Tasmota Scripts

Easy development of Tasmota scripts with syntax highlighting. You may edit scripts with indents and any number of comments and with a key press (`CMD R`) transfer the script to an ESP via WLAN. Before transferring, all unnecessary characters and comments are removed. The script is immediately executed.

I developed this editor for other tasks many years ago. This project is based on a cross-compiler that generates macOS and Windows programs in one action. 

- The macOS version can be started by clicking.
- The Windows version is an app folder, requiring following manual settings to be started.
    -   Create a shortcut to the `.exe` file in the subfolder `contents/windows`, click this shortcut for starting the editor. 

Note: The folder structure must remain unchanged, because otherwise the app will no longer work!

The editor recognizes Tasmota scripts when the file name has the suffixes `.tas` (recommended) or `.txt` when additionally the string `>D` is found in the first line. 
Note: file extension (i.e. `.tas`) must be visible in the window header bar; suppress file suffix must be turned off!

The ESP’s IP number has to be specified (in any line) by:
```
IP=xxx.xxx.xxx
```

The ESP script buffer size may also be specified by:
```
SB=xxxx:cccc
```
where `xxxx` is the uncompressed size of the script buffer, and `cccc` is the compressed space (usually 1535 bytes) (if compression is used).

If specified, the editor shows the resulting stripped-down size and (if specified, Unishox compressed size) in the line display as characters are entered. If the line display turns red, the size is too large to be transferred to the ESP.

Example:
```
Line: $4: 213$
> D
SB=4000
IP=192.168.178.232
p: P=0
p: PON=0.01
p: T1=10
p: MYP=1000
```

On an ESP8266, you have at least 2 options for script buffer with standard 1M flash:
- 2560 bytes and compressed 1535 bytes (default configuration): `SB=2560:1535`
- Or if you have 4M Flash: `SB=4096`

And in `user_config_override`:
```c
#define USE_UFILESYS
```

---

## Additional Configuration

And select a different linker file in `platform_override`:
```
board_build.ldscript = 
    eagle.flash.1m.ld
    eagle.flash.4m2m.ld
```

On an ESP32, besides the default option, you may use, e.g.:
```
SB=8192
```

And in `user_config_override`:
```c
#define USE_UFILESYS
#define UFSYS_SIZE 8192
```

Optionally, you may transfer files to the ESP’s file system by:
```
UFILES=file1.txt,PICS/test.jpg
```
You may specify files separated by a colon. Files must be in the same folder as the source file itself or in a subfolder.

`CMD R` or `Window->Run` in the Menu transfers the file to the ESP. On every transfer, a stripped-down copy and a colorized copy of the file are saved to the documents folder named `compressed_script.txt` and `color_script.rtf`.

With `Window->Export`, you may also export these files without transferring.

The syntax color items contain all scripting variables, all Tasmota emits, and most JSON strings. The syntax definition files are simple text files that may be modified at any time. In the syntax files, any comment lines (starting with `;`) are allowed. Optionally, in the very first line, a color may be defined which overwrites the default color (`#RRGGBB` hex format).

There are currently 5 categories:
- `tasmota_keywords.txt`: Scripter keywords
- `tasmota_numvars.txt`: Numeric vars
- `tasmota_strvars.txt`: String vars
- `tasmota_json.txt`: Tasmota JSON variables
- `tasmota_specvars.txt`: Tasmota commands

In the submenu `Various->Start`, you can find a utility called **Fonteditor**. There you find 3 utility programs:

1. **Edit EPD and glcd.c font files**  
   This is a font editor with which you can edit C source code files of EPD font definition files directly to adapt the fonts to your needs. However, you must recompile the Tasmota source to install the modified fonts.

---

2. **Convert a picture to RGB16 format**  
   This allows you to convert, e.g., a JPG picture to an RGB16 picture, which is the only format supported on ESP8266 displays and is also preferred on ESP32 because it can be displayed without any conversion (no extra RAM needed).

3. **Convert GFX .h font files to binary files**  
   This converts a GFX font header file to a binary file which can be loaded by the `displaytext RAMFONT` directive.
```
