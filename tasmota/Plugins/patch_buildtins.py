# insert plugin sections into linker file
Import("env")

import os
import shutil
import pathlib
import tasmotapiolib
import binascii
from os.path import join

try:
    import numpy as np
except ImportError:
    env.Execute("$PYTHONEXE -m pip install numpy")


# give object file
file = 'xsns_46_MLX90614_test.cpp.o'

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")

cfunc = ""
org_pos = 0
copy_pos = 0

def array_find(index, arr1, arr2):
    m = len(arr1)
    n = len(arr2)
    for i in range(index, m):
        if arr1[i]==arr2[0]:
            copy = arr1[i:i+n]
            if copy == arr2:
                #copy = arr1[i:i+n+3]
                #print(copy)
                if arr1[i+n]==0:
                    #print("found org")
                    global org_pos
                    org_pos = i
                    return i
                else:
                    istart = 0
                    istop = 0
                    for x in range(i, i - 5, -1):
                        if arr1[x] == 0:
                            istart = x + 1
                            break
                    for x in range(i, i + n + 4, 1):
                        if arr1[x] == 0:
                            istop = x
                            break
                    global cfunc
                    cfunc = arr1[istart:istop]
                    #print("found: "+str(cfunc))
                    global copy_pos
                    copy_pos = istart
                    return istart
    return -1

    # always 2 functions to patch
def find_and_patch(source, sub):
    global org_pos
    org_pos = 0
    global copy_pos
    copy_pos = 0
    offset = array_find(0, source, sub)
    offset = array_find(offset + len(sub), source, sub)
    #print(org_pos,copy_pos)
    if org_pos==0 or copy_pos==0:
        print(sub.decode("utf-8")+" already patched or not needed")
    else:
        print("patching: "+sub.decode("utf-8"))
        patch = sub
        # patch with leading x
        patch[0] = 120
        patch.append(0)
        #print(patch)
        source[copy_pos:copy_pos+len(patch)] = patch
        source[org_pos:org_pos+len(patch)] = patch


def patch_builtins(source, target, env):
    print("patching buildtins: "+file)
    path=str(target[0])
    directory = "/".join(list(path.split('/')[0:-1]))
    dpath = directory+"/src/Plugins/"
    fpath = dpath + file
 
    with open(fpath, mode='rb') as f:
        source = f.read()
        f.close()
    source=bytearray(source)

    # list patches here
    patches = ["__addsf3", "__subsf3", "__mulsf3", "__divsf3", "__nesf2"]

    for x in patches:
        sub = bytearray(x, 'utf-8')
        find_and_patch(source, sub)

    #fpath = dpath + "test.o"

    with open(fpath, mode='wb') as f:
        f.write(source)
        f.close()


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [patch_builtins])

