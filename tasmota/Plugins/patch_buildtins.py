# insert plugin sections into linker file
Import("env")

import os
import shutil
import pathlib
import tasmotapiolib
import binascii
from os.path import join


# get filename from user_config override
hpath = "tasmota/user_config_override.h"
with open(hpath, mode='r') as f:
        header = f.read()
        f.close()

hparts = header.split("CURRENT_FILE ")
fname = hparts[1].splitlines()

file = fname[0].strip()
if file == '':
    # give object file
    file = 'xsns_46_MLX90614_test.cpp.o'

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")

cfunc = ""
org_pos = 0
copy_pos = 0

def array_find(index, arr1, arr2, flag):

    arr3 = [0]

    if flag==1:
        # org has undefined char in the middle
        # have to compare part 1 and then check for part2
        ssub = arr2.decode("utf-8")
        ssp = ssub.split('_')
        #print(ssp[0])
        arr2 = bytearray(ssp[0], 'utf-8')
        arr3 = bytearray(ssp[1], 'utf-8')

    m = len(arr1)
    n = len(arr2)
    for i in range(index, m):

        if arr1[i]==arr2[0]:
            copy = arr1[i:i+n]
            if copy == arr2:
                #copy = arr1[i:i+n+3]
                #print(copy)
                global org_pos
                global copy_pos
                global cfunc
                if flag==0:
                    if arr1[i+n]==0:
                        #print("found org")
                        
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
                        cfunc = arr1[istart:istop]
                        #print("found: "+str(cfunc))
                        copy_pos = istart
                        return istart
                else:
                    # class
                    if flag==2:
                        # find copy
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
                        copy_pos = istart
                        return istart
                    else:
                        # found part 1, compare part 2
                        x=len(arr3)
                        copy = arr1[i+n+2:i+n+2+x]
                        istart = 0
                        if copy==arr3:
                            for x in range(i, i - 5, -1):
                                if arr1[x] == 0:
                                    istart = x + 1
                                    break
                            org_pos = istart
                            print("found: ",arr2.decode("utf-8")+"_"+arr3.decode("utf-8"))
                            return istart
    return -1

    # always 2 functions to patch
def find_and_patch(source, sub):
    global org_pos
    org_pos = 0
    global copy_pos
    copy_pos = 0

    if sub[0] != ord('_'):
        #print ("patch class")
        ssub = sub.decode("utf-8")
        spstr = ssub.split('_')
        # find org
        sub = bytearray(spstr[2]+'_'+spstr[1], 'utf-8')
        offset = array_find(0, source, sub, 1)
        # find copy
        sub = bytearray(spstr[0]+'_'+spstr[1], 'utf-8')
        offset = array_find(offset + len(sub), source, sub, 2)
    else:
        offset = array_find(0, source, sub, 0)
        offset = array_find(offset + len(sub), source, sub, 0)
    
    #print(org_pos,copy_pos)
    if org_pos==0 or copy_pos==0:
        print(sub.decode("utf-8")+" already patched or not needed")
        if (org_pos > 0):
            print("missing implementation of: "+sub.decode("utf-8"))
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
    patches = ["__addsf3", "__subsf3", "__mulsf3", "__divsf3", "_Znwj", "_ZdlPv",\
    "__floatsisf", "__floatunsisf", "__floatundisf", "__fixsfsi", "__fixunssfsi",\
    "__floattidf", "__floatuntidf", "__floatsidf", "__floatunsidf", "__fixdfdi",\
    "__fixunsdfsi", "__fixdfti", "__extendsfdf2", "__adddf3", "__subdf3",\
    "__muldf3", "__divdf3", "__ltdf2", "__gtdf2", "__nedf2", "__eqdf2",\
    "__gtsf2", "__gesf2", "__ltsf2", "__lesf2", "__eqsf2", "__nesf2", "__unordsf2",\
    "TWI_beginTransmission_TwoWire", "TWI_endTransmission_TwoWire","TWI_requestFrom_TwoWire",\
    "TWI_write_TwoWire","TWI_read_TwoWire"]

    for x in patches:
        find_and_patch(source, bytearray(x, 'utf-8'))

    #fpath = dpath + "test.o"

    with open(fpath, mode='wb') as f:
        f.write(source)
        f.close()


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [patch_builtins])

