Import('env')
import pathlib
import os

# convert with an esp8266 project
# copy tasmota driver into plugins directory
# reformat files with https://formatter.org/cpp-formatter
# correct or remove #include entries
# edit file name below, run script, driver.cpp is created
# now edit remaining issues

# edit this path
#file = 'xsns_70_veml6075.ino'
file = 'xsns_05_ds18x20.ino'


platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")

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

source = source.replace("unsigned char", "uint8_t")
source = source.replace("TasmotaGlobal.uptime", "GetTasmotaGlobal(4)")
source = source.replace("TasmotaGlobal.tele_period", "GetTasmotaGlobal(1)")


deinit = module.upper() + "_Deinit"
index = source.rindex("/****")
part1 = source[0:index]
part2 = source[index:]

# insert deinit
source = part1 + "void " + deinit + "(void) {\nRETMEM\n}\n" + part2

# loop for functions in source
lines = source.split("\n")

func_list = []
func_args = []
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
        i2 = cline.find(")")
        p3 = cline[index:i2+1]
        func_list.append(p2)
        func_names.append(p2[-1])
        func_args.append(p3)

def_func = []

fcnt = 0
for func in func_list:
    if len(func) > 1 and len(func) < 3 :
        if len(func[1]) > 0 :
            if func[0][0] != '#':
                func.append(func_args[fcnt])
                def_func.append(func)
                print(func)
    fcnt += 1

# insert these into file
for func in def_func:
    sstr = func[0] + " " + func[1] + '('
    index = source.find(sstr)
    part1 = source[0:index]
    part2 = source[index:]
    index = part2.find(')')+3
    part3 = part2[0:index]
    part4 = part2[index:]

    if sstr.find("mod_func_execute") < 0 :
        if sstr.find("Detect") > 0 :
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
istr += "MODULE_DESCRIPTOR(\"" + module.upper() + "\"," + mod_type + "," + "1<<16|2" + ",\"\",0,\"\",0,\"\",0,\"\",0)\n"

for func in def_func:
    fname = "MODULE_PART " + func[0] + " " + func[1] + func[2]
    #print(fname)
    istr += fname + ";\n"

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
istr += "#include \"../Tasmota/include/i18n.h\"\n"

source = source.replace("#ifdef "+muse, istr)

print(muse)

# search for MODULE_MEMORY
'''
typedef struct {
  uint8_t veml6075_active = 0;
  veml6075configRegister veml6075Config;
  VEML6075STRUCT veml6075_sensor;
} MODULE_MEMORY;
'''

memvars = []

index = source.find("MODULE_MEMORY")
if index < 0 :
    print("MODULE_MEMORY missing")
else :
    part1 = source[0:index]
    index = part1.rindex("{")
    part1 = part1[index:]
    # memory structure
    lines = part1.split("\n")
    cnt = 1
    while cnt < len(lines) - 1:
        oline = lines[cnt].strip()[:-1]
        oline = oline.split(" ")
        cnt += 1
        memvars.append(oline[1])

istr = ""
for func in memvars:
    brind = func.find("[")
    if brind > 0 :
        func = func[0:brind]
    istr += "#define " + func + " mem->" + func + "\n"

source = source.replace("MODULE_MEMORY;", "MODULE_MEMORY;\n\n" + istr)

source = source.replace("XdrvMailbox.", "XdrvMailbox->")


index = source.rindex("}")
index = source.rfind("}", 0, index)
index = source.rfind("\n", 0, index)

part1 = source[0:index]
part2 = source[index:]

source = part1 + "\n\t\t\tcase FUNC_DEINIT:\n\t\t\t\t" + deinit + "();\n\t\t\t\tbreak;\n" + part2

fwp = open(dfname, "w")
fwp.write(source)
fwp.close()

print("scan ready")
