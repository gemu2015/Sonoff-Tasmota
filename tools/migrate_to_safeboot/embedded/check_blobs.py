"""Pre-build hook: refuses to compile if the embedded blob headers are
missing or stale. Saves the user from shipping a migrator with a
zero-length safeboot blob, which would brick every device it touches.

Invoked by platformio.ini's `extra_scripts = pre:embedded/check_blobs.py`.
PlatformIO doesn't define `__file__` for these scripts, so we resolve
the project directory via env.subst() and append the embedded/ subdir.
"""
import os
import sys

Import("env")

# PROJECT_DIR is set by PIO to the directory containing platformio.ini.
project_dir = env.subst("$PROJECT_DIR")
here = os.path.join(project_dir, "embedded")

required = ["safeboot_blob.h", "partitions_blob.h"]
for name in required:
    path = os.path.join(here, name)
    if not os.path.exists(path):
        print(f"\nERROR: {path} is missing.")
        print(f"Run `embedded/build_blobs.sh` to generate it from a fresh")
        print(f"safeboot.bin and the new partition CSV.\n")
        sys.exit(1)
    if os.path.getsize(path) < 1024:
        print(f"\nERROR: {path} looks suspiciously small "
              f"({os.path.getsize(path)} bytes). Regenerate it.\n")
        sys.exit(1)

print(f"check_blobs.py: embedded blobs present, OK")
