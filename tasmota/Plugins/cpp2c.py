# convert c++ to c helper
Import('env')
import pathlib
import os

# convert with an esp8266 project
# copy c++ files to new folder
# reformat files with https://formatter.org/cpp-formatter
# correct or remove #include entries
# set path to your new folder
# run script
# now edit remaining issues

# edit this path
path = '/Users/gerhardmutz1/Desktop/BresserWeatherSensorReceiver-main/RadioLib-master/src/modules/CC1101/CC1101.cpp'
#path = '/Users/gerhardmutz1/Desktop/BM8563_RTC/src/BM8563.cpp'

# if only 1 class set to empty string
searchclass ='CC1101'

dpath = "tasmota/plugins/"
fname =  pathlib.PurePath(path).stem
dfname = fname
fname += "_cpp.txt"
dfname += "_c.h"

print("preprocess: " + fname)

platform = env.PioPlatform()
board = env.BoardConfig()
mcu = board.get("build.mcu", "esp32")


intermediate =  dpath + fname

# preprocess, also adds all header files 
if mcu == "esp8266":
    env.Execute("xtensa-lx106-elf-cpp " + path + " > " +intermediate )
if mcu == "esp32":
    env.Execute("xtensa-esp32-elf-cpp " + path + " > " +intermediate )

# prepro done, scan for class
with open(intermediate) as f:
    data = f.read()
    f.close()

index = data.find("class "+searchclass)
check = data[index+6:index+20]
source = data[index+6:]

cname = check.split('{')[0]
cname = cname.split(' ')[0]
cname = cname.replace(" ", "")
cname = cname.replace(":", "")
cname = cname.rstrip()

print("found class: " + cname)
#class CC1101: public PhysicalLayer {
#    CC1101: public

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

# set Wire prefix
wire_prefix = "Wire."
#wire_prefix = "myWire->"

# replace keywords
source = source.replace(wire_prefix+"beginTransmission", "beginTransmission")
source = source.replace(wire_prefix+"endTransmission()", "endTransmission(true)")
source = source.replace(wire_prefix+"write", "write")
source = source.replace(wire_prefix+"read", "read")
source = source.replace(wire_prefix+"requestFrom", "requestFrom")
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

os.remove(intermediate)

#print(openbr)
#print(closebr)