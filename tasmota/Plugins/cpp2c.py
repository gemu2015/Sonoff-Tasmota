# convert c++ to c helper
Import('env')
import pathlib

# convert with an esp8266 project
# copy c++ files to new folder
# reformat files with https://formatter.org/cpp-formatter
# correct or remove #include entries
# set path to your new folder
# run script
# now edit remaining issues

# edit this path
#path = '/Users/gerhardmutz1/Desktop/vl53l0x-arduino-1.02/VL53L0X.cpp'
path = '/Users/gerhardmutz1/Desktop/BM8563_RTC/src/BM8563.cpp'

dpath = "tasmota/plugins/"
fname =  pathlib.PurePath(path).stem
dfname = fname
fname += "_cpp.txt"
dfname += "_exe.h"

print("preprocess: " + fname)

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")


# preprocess, also adds all header files 
env.Execute("xtensa-lx106-elf-cpp " + path + " > " + dpath + fname )

# prepro done, scan for class
with open(dpath + fname) as f:
    data = f.read()
    f.close()

index = data.find("class")
check = data[index+6:index+20]
source = data[index+6:]

cname = check.split('{')[0]
cnam = cname.replace(" ", "")
cname = cname.rstrip()

print("found class: " + cname)

cl_class = cname + "::"
cl_func = cname + "_"

# loop for functions in source
lines = source.split("\n")

func_list = []
func_names = []

print("scan class")
cnt = 0
openbr = 0
closebr = 0
while cnt < len(lines):
    oline = lines[cnt]
    cline = oline.strip()
    #print(cline)
    openbr += cline.count("{")
    closebr += cline.count("}")
    cnt+=1
    if cline.count("(") :
        # function definition, get function name
        index = cline.find("(")
        p1 = cline[:index]
        p2 = p1.split(" ")
        func_list.append(p2)
        func_names.append(p2[-1])

    #if len(oline) > 0 :
        #fwp.write(oline+'\n')

    if openbr > 0 :
        if openbr == closebr :
            # class definition closed
            break

print("class definition ready")

print("scan body")

opnold = openbr
while cnt < len(lines):
    # read rest of file
    oline = lines[cnt]
    cline = oline.strip()
    #print(cline)
    openbr += cline.count("{")
    closebr += cline.count("}")
    cnt+=1
    #if len(oline) > 0 :
        #fwp.write(oline+'\n')
    #if opnold != openbr :
        #if openbr == closebr :
            #print("new func")

print("body scan ready")

#insert module definition 
for func in func_list:
    fdef = ""
    for part in func :
        fdef += ' ' + part
    #print(fdef)
    source = source.replace(fdef + '(', "MODULE_PART " + fdef + '(')

# replace class names
source = source.replace(cl_class, "")

for func in func_names:
   fname = cl_func + func + '('
   #print(fname)
   source = source.replace(func + '(', fname)


# replace keywords, Wire vectors -> must be replaced with Texteditor
source = source.replace("Wire.beginTransmission", "beginTransmission")
source = source.replace("Wire.endTransmission()", "endTransmission(true)")
source = source.replace("Wire.write", "write")
source = source.replace("Wire.read", "read")
source = source.replace("Wire.requestFrom", "requestFrom")
source = source.replace("public:", "")
source = source.replace("private:", "")


# split again
lines = source.split("\n")

#write destination file
fwp = open(dpath + dfname, "w")

#remove empty lines
cnt = 0
while cnt < len(lines):
    oline = lines[cnt]
    cnt+=1
    if len(oline) > 0 :
        if oline[0] != '#' :
            fwp.write(oline+'\n')
    
fwp.close()

print("c file created")

#print(openbr)
#print(closebr)