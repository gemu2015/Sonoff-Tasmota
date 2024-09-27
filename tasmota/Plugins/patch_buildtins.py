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

def string_to_array(s):
    return [c for c in s]

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
                    print("found org")
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
                    print("found: "+str(cfunc))
                    global copy_pos
                    copy_pos = istart
                    return istart
    return -1

def isSubset(arr1, arr2):
    m = len(arr1)
    n = len(arr2)
    i = 0
    j = 0
    for i in range(n):
        for j in range(m):
            if(arr2[i] == arr1[j]):
                break
         
        if (j == m):
            return -1
    return j

def patch_builtins(source, target, env):
    print("patching buildtins: "+file)
    path=str(target[0])
    directory = "/".join(list(path.split('/')[0:-1]))
    dpath = directory+"/src/Plugins/"
    fpath = dpath + file
    #print(fpath)

    with open(fpath, mode='rb') as f:
        source = f.read()
        f.close()

    source=bytearray(source)

    print(len(source))
 
    sub = bytearray("__addsf3", 'utf-8')

    # always 2 functions to patch
    global org_pos
    orgpos = 0
    global copy_pos
    copy_pos = 0
    offset = array_find(0, source, sub)
    offset = array_find(offset + len(sub), source, sub)

    if org_pos==0 or copy_pos==0:
        print("already patched")

    print(org_pos,copy_pos)

    global cfunc
    # replace org with copy and invalidate org
    if org_pos > copy_pos:
        # move part 1
        len1 = copy_pos
        len2 = org_pos - copy_pos - len(cfunc)
        len3 = len(source) - org_pos - len(sub)
        print(len1,len(cfunc),len2,len(sub),len3)

        print(len1+len(cfunc)+len2+len(sub)+len3)
             
        sum = len1+len(cfunc)+len2+len(sub)+len3
        print(len(source))

        part1 = bytearray(len1)
        part2 = bytearray(len2)
        part3 = bytearray(len3)

        part1[0:len1] = source[0:len1]
        part2[0:len2] = source[copy_pos + len(cfunc):org_pos]
        part3[0:len3] = source[org_pos + len(sub):len(source)]

        source[copy_pos:len(sub)] = sub
        source[copy_pos+len(sub):len2] = part2
        source[copy_pos+len(sub)+len2:] = cfunc
        source[copy_pos+len(sub)+len2+len(cfunc):] = part3


        #fpath = dpath + "test.o"

        with open(fpath, mode='wb') as f:
            f.write(source)
            f.close()

        print (len(part1),len(part2),len(part3))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [patch_builtins])

