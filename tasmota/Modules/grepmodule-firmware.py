Import("env")

import os
import shutil
import tasmotapiolib
import binascii


#MODULE_SYNC = 0x55aaFC4A



if env["PIOPLATFORM"] != "espressif32" :

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
                    start = 1
                    #print("found start sync")
                    dummy = fp.read(12)
                    xname = fp.read(16)
                    fname = xname.decode('ascii') 
                    name = fname.replace('\x00','')
                    mod_file = dir_path + "/" + name + ".bin"
                    if os.path.isfile(mod_file):
                        os.remove(mod_file)
                    fwp = open(mod_file, "wb")
                    fwp.write(msync)
                    fwp.write(dummy)
                    fwp.write(xname)
                    size += 24

                if msync[0] == 0x55 and msync[1] == 0xaa and msync[2] == 0xfc and msync[3] == 0x4a:
                    start = 2
                    #print("found end sync")
                    fwp.close()
                    break
                    
                
        if start == 2:
            print("extracted module: "+name)
            if size > 4096:
                print("module size error")


    if not tasmotapiolib.is_env_set(tasmotapiolib.DISABLE_BIN_GZ, env):
        env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [grep_module])
