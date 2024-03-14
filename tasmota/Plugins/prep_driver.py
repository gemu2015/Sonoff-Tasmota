# convert tasmota driver
Import('env')
import pathlib
import os

# convert with an esp8266 project
# reformat files with https://formatter.org/cpp-formatter
# correct or remove #include entries
# run script
# now edit remaining issues

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")

# edit this path
file = 'xsns_70_veml6075.ino'

dpath = "tasmota/plugins/"
fpath =  dpath + file

dfname = dpath + file.split(".")[0] + ".cpp"

with open(fpath) as f:
    source = f.read()
    f.close()

fpart = file.split("_")
dnum = "Xsns"+fpart[1]
type = fpart[0]
source = source.replace(dnum, "mod_func_execute")
module = fpart[2]
module = module.split(".")[0]

muse = "USE_" + module.upper()

# loop for functions in source
lines = source.split("\n")

func_list = []
func_names = []

print("scan for functions")
cnt = 0
openbr = 0
closebr = 0
while cnt < len(lines):
    oline = lines[cnt]
    cline = oline.strip()
    #print(cline)
    cnt+=1
    if cline.count("(") :
        # function definition, get function name
        index = cline.find("(")
        p1 = cline[:index]
        p2 = p1.split(" ")
        func_list.append(p2)
        func_names.append(p2[-1])


def_func = []

for func in func_list:
   if len(func) > 1 and len(func) < 3 :
    if len(func[1]) > 0 :
        fname = func
        def_func.append(func)

# insert these into file

for func in def_func:
    sstr = func[0] + " " + func[1]
    index = source.find(sstr)
    part1 = source[0:index]
    part2 = source[index:]
    index = part2.find(')')+3
    part3 = part2[0:index]
    part4 = part2[index:]

    if sstr.find("mod_func_execute")<0 :
        if sstr.find("Detect")>0 :
            regvar = "\nALLOCMEM\n"
        else :
            regvar = "\nSETREGS\n"
        source = part1 + part3 + regvar + part4


if type == "xsns" :
   mod_type = "MODULE_TYPE_SENSOR"
if type == "xdrv" :
   mod_type = "MODULE_TYPE_DRIVER"

istr = "/********************************************************************************************/\n"
istr += "PUSH_OPTIONS\n"
#istr += "MODULE_DESCRIPTOR(\"MP3PLAYER\"," + mod_type + "," + mod_rev + ",\"\",0,\"\",0,\"\",0,\"\",0)\n"
istr += "MODULE_DESCRIPTOR(\"" + module + "\"," + mod_type + "," + "1<<16|2" + ",\"\",0,\"\",0,\"\",0,\"\",0)\n"

for func in def_func:
    fname = "MODULE_PART " + func[0] + " " + func[1]
    #print(fname)
    istr += fname + "\n"

istr += "MODULE_END\n"
istr += "/********************************************************************************************/\n"

func = def_func[0]
sstr = func[0] + " " + func[1]
index = source.find(sstr)
part1 = source[0:index]
part2 = source[index:]
source = part1 + istr + part2

lastif = source.rindex("#endif")
part1 = source[0:lastif]
part2 = source[lastif:]
source = part1 + "\nPULL_OPTIONS\n" + part2


istr = "#include \"tasmota_options.h\"\n#ifdef " + muse + "_MOD\n"
istr += "#include \"module.h\"\n"
istr += "#include \"module_defines.h\"\n"

source = source.replace("#ifdef "+muse, istr)




fwp = open(dfname, "w")
fwp.write(source)
fwp.close()

print("scan ready")
