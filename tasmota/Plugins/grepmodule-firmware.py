Import("env")

import os
import shutil
import pathlib
import tasmotapiolib
import binascii
from os.path import join

#MODULE_SYNC = 0x55aaFC4A

#if env["PIOPLATFORM"] == "espressif32" :
if 1==1 :

    def grep_module(source, target, env):
        #print("searching binary for module ")
        # create string with location and file names based on variant
        bin_file = tasmotapiolib.get_final_bin_path(env)
        dir_path = os.path.dirname(bin_file)
    
        start = 0
        size = 0
        with open(bin_file, "rb") as fp:
            while (msync := fp.read(4)):
                if start == 1:
                    size += 4

                if start == 1:
                    fwp.write(msync)

                if msync[0] == 0x4a and msync[1] == 0xfc and msync[2] == 0xaa and msync[3] == 0x55:
                    xarch = fp.read(4)
                    arch = int.from_bytes(xarch, "little") & 0xf
                    xtype = fp.read(4)
                    type = int.from_bytes(xtype, "little")
                    if arch == 0 and type <= 4:
                        print("found start sync")
                        start = 1
                        dummy = fp.read(4)
                        xname = fp.read(16)
                        fname = xname.decode('ascii') 
                        name = fname.replace('\x00','')
                        mod_file = dir_path + "/" + name + ".bin"
                        if os.path.isfile(mod_file):
                            os.remove(mod_file)
                        fwp = open(mod_file, "wb")
                        fwp.write(msync)
                        fwp.write(xarch)
                        fwp.write(xtype)
                        fwp.write(dummy)
                        fwp.write(xname)
                        size += 28
                    if arch == 1 and type <= 4:
                        print("found start sync")
                        start = 1
                        dummy = fp.read(4)
                        xname = fp.read(16)
                        fname = xname.decode('ascii') 
                        name = fname.replace('\x00','')
                        mod_file = dir_path + "/" + name + "_32.bin"
                        if os.path.isfile(mod_file):
                            os.remove(mod_file)
                        fwp = open(mod_file, "wb")
                        fwp.write(msync)
                        fwp.write(xarch)
                        fwp.write(xtype)
                        fwp.write(dummy)
                        fwp.write(xname)
                        size += 28
                    if arch == 2 and type <= 4:
                        print("found start sync")
                        start = 1
                        dummy = fp.read(4)
                        xname = fp.read(16)
                        fname = xname.decode('ascii') 
                        name = fname.replace('\x00','')
                        mod_file = dir_path + "/" + name + "_32r.bin"
                        if os.path.isfile(mod_file):
                            os.remove(mod_file)
                        fwp = open(mod_file, "wb")
                        fwp.write(msync)
                        fwp.write(xarch)
                        fwp.write(xtype)
                        fwp.write(dummy)
                        fwp.write(xname)
                        size += 28

                if start > 0 and msync[0] == 0x55 and msync[1] == 0xaa and msync[2] == 0xfc and msync[3] == 0x4a:
                    start = 2
                    size += 4
                    print("found end sync")
                    # set module size 
                    fwp.seek(40)
                    sizeb = size.to_bytes( 4, "little" )
                    fwp.write(sizeb)
                    fwp.close()
                    break
                    
                
        if start == 2:
            print("extracted module: "+name)
            #if size > 8192:
            print(f"module size: {size}")


    #if not tasmotapiolib.is_env_set(tasmotapiolib.DISABLE_BIN_GZ, env):
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [grep_module])
