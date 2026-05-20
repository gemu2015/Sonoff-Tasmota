#!/usr/bin/env python3
"""Bundle TinyC IDE into a single HTML file for ESP32 device upload.

Usage: python3 bundle.py [--gzip]

Output: tinyc_ide.html (and tinyc_ide.html.gz if --gzip)
Upload to device filesystem via Tasmota file manager.
"""

import re
import sys
import os
import glob as glob_mod
import gzip as gzip_mod

os.chdir(os.path.dirname(os.path.abspath(__file__)))

# JS modules in dependency order
MODULES = [
    'src/preprocessor.js',
    'src/opcodes.js',
    'src/lexer.js',
    'src/parser.js',
    'src/codegen.js',
    'src/vm.js',
    'src/compiler.js',
]

# Examples: (filename_without_ext, display_name)
# Order determines dropdown order. Files are read from examples/*.tc
EXAMPLES = [
    ('editor',         'Hello World'),
    ('blink',          'Blink LED'),
    ('fibonacci',      'Fibonacci'),
    ('strings',        'Strings'),
    ('sensor_read',    'Sensor Read'),
    ('sort',           'Bubble Sort'),
    ('file_io',        'File I/O'),
    ('callbacks',      'Callbacks'),
    ('chart',          'Google Chart'),
    ('live_chart',     'Live Chart (_Q)'),
    ('udp',            'UDP Multicast'),
    ('sht31',          'SHT31 Sensor'),
    ('bmx280',         'BMx280 Sensor'),
    ('ads1115',        'ADS1115 ADC'),
    ('vl53l0x',        'VL53L0X Distance'),
    ('mlx90614',       'MLX90614 IR Temp'),
    ('tcs34725',       'TCS34725 RGB Color'),
    ('veml6075',       'VEML6075 UV Sensor'),
    ('scd30',          'SCD30 CO2 Sensor'),
    ('sgp30',          'SGP30 eCO2/TVOC'),
    ('sps30',          'SPS30 Particulate'),
    ('ccs811',         'CCS811 Air Quality'),
    ('ld2410',         'LD2410 Presence'),
    ('dysv17f',        'DY-SV17F MP3 Player'),
    ('max31855',       'MAX31855 Thermocouple'),
    ('epaper29',       'EPaper 2.9" Display'),
    ('watch_demo',     'Watch Variables'),
    ('chart_types',    'Chart Types Demo'),
    ('webcall_demo',   'WebUI Main Page'),
    ('webui_demo',     'WebUI Dashboard'),
    ('multipage_demo', 'WebUI Multi-Page'),
    ('web_handler',    'Custom Web Handler'),
    ('bresser',        'Bresser Weather (CC1101)'),
    ('bresser_chart',  'Bresser + 24h Charts'),
    ('benchmark',      'Benchmark'),
    ('display_demo',   'Display Drawing'),
    ('touch_buttons',  'Touch Buttons'),
    ('homekit_demo',   'HomeKit Demo'),
    ('homekit_office', 'HomeKit Office'),
    ('camera',         'ESP Camera (DFRobot)'),
]

print("Bundling TinyC IDE...")

# Read and process each JS module
js_parts = []
for path in MODULES:
    with open(path, 'r') as f:
        code = f.read()

    # Strip import lines: import { ... } from './xxx.js';
    code = re.sub(
        r"^import\s+\{[^}]*\}\s+from\s+['\"]\.\/[^'\"]+['\"];?\s*$",
        '', code, flags=re.MULTILINE
    )

    # Strip 'export ' prefix from declarations (including 'export async function')
    code = re.sub(
        r"^export\s+(async\s+)?(class|function|const|let|var)\s",
        lambda m: (m.group(1) or '') + m.group(2) + ' ', code, flags=re.MULTILINE
    )

    # Strip re-export lines: export { ... } from './xxx.js';
    code = re.sub(
        r"^export\s+\{[^}]*\}\s+from\s+['\"]\.\/[^'\"]+['\"];?\s*$",
        '', code, flags=re.MULTILINE
    )

    basename = os.path.basename(path)
    js_parts.append(f"// --- {basename} ---\n{code.strip()}")

bundled_js = '\n\n'.join(js_parts)

# ─── Load examples from files ───────────────────────────────
examples_js_parts = []
examples_options = []
examples_loaded = 0

for key, display_name in EXAMPLES:
    filepath = f'examples/{key}.tc'
    if not os.path.exists(filepath):
        print(f"  WARNING: {filepath} not found, skipping")
        continue

    with open(filepath, 'r') as f:
        source = f.read().rstrip()

    # Escape backticks and ${} for JS template literal
    source = source.replace('\\', '\\\\').replace('`', '\\`').replace('${', '\\${')

    examples_js_parts.append(f'            {key}: `{source}`')
    examples_options.append(
        f'                <option value="{key}">{display_name}</option>')
    examples_loaded += 1

examples_js = ',\n'.join(examples_js_parts)
examples_html = '\n'.join(examples_options)

print(f"  Loaded {examples_loaded} examples from examples/*.tc")

# Read index.html
with open('index.html', 'r') as f:
    html = f.read()

# Inject examples into placeholders
html = html.replace('/*EXAMPLES_DATA*/ ', examples_js + '\n        ')
html = html.replace('<!--EXAMPLES_OPTIONS-->', examples_html)

# Replace the ES module script with inlined code
# Find the import line and replace the script tag + import with inlined code
match = re.search(
    r'<script type="module">\s*import\s+\{[^}]*\}\s+from\s+[\'"]\.\/src\/compiler\.js[\'"];',
    html
)
if not match:
    print("ERROR: Could not find module import in index.html")
    sys.exit(1)

# The rest of index.html (after the import) becomes inline JS inside <script>.
# Any literal </script> in JS strings (e.g. in EXAMPLES) would prematurely
# close the <script> tag.  Escape them: </script → <\/script (valid JS, invisible to HTML parser).
rest_of_html = html[match.end():]
# Only escape inside the <script> block (up to the real closing </script>)
# Find the real closing tag — it's the last </script> in the file
last_close = rest_of_html.rfind('</script>')
if last_close > 0:
    js_part = rest_of_html[:last_close]
    after_part = rest_of_html[last_close:]
    js_part = js_part.replace('</script>', '<\\/script>').replace('</Script>', '<\\/Script>')
    rest_of_html = js_part + after_part

html = html[:match.start()] + '<script>\n' + bundled_js + '\n' + rest_of_html

# Add auto-detection for device-hosted mode
# Insert after the "Restore device IP" block so it fills in hostname when served from device
auto_detect = """
        // Auto-detect: when served from device, use device hostname
        (function() {
            var params = new URLSearchParams(window.location.search);
            if (!params.has('device') && window.location.hostname !== 'localhost'
                && window.location.hostname !== '127.0.0.1') {
                var ip = document.getElementById('deviceIp');
                if (ip && !ip.value) ip.value = window.location.hostname;
            }
        })();
"""

# Find the end of the "Restore device IP" block to insert after it
anchor = "// Close IDE and return to Tasmota"
if anchor in html:
    html = html.replace(anchor, auto_detect + "\n        " + anchor)
else:
    print("WARNING: Could not find insertion point for auto-detect code")

# Write bundled HTML
with open('tinyc_ide.html', 'w') as f:
    f.write(html)

size = len(html.encode('utf-8'))
print(f"Created tinyc_ide.html ({size:,} bytes)")

# Always create gzip version (device serves .gz)
with open('tinyc_ide.html', 'rb') as f_in:
    data = f_in.read()
with gzip_mod.open('tinyc_ide.html.gz', 'wb', compresslevel=9) as f_out:
    f_out.write(data)
gz_size = os.path.getsize('tinyc_ide.html.gz')
print(f"Created tinyc_ide.html.gz ({gz_size:,} bytes)")

# Copy .gz to Tasmota repo
import shutil
TASMOTA_TINYC = os.path.expanduser(
    '~/Desktop/Smart-Home/Tasmota/Development/Sonoff-Tasmota/tasmota/tinyc')
if os.path.isdir(TASMOTA_TINYC) and os.path.exists('tinyc_ide.html.gz'):
    shutil.copy2('tinyc_ide.html.gz', TASMOTA_TINYC)
    print(f"Copied tinyc_ide.html.gz -> {TASMOTA_TINYC}/")

print("Done! Upload tinyc_ide.html (or .gz) to device filesystem.")
