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
file = 'xsns_70_veml6075.cpp'

dpath = "tasmota/plugins/"
fpath =  dpath + file

dfname = dpath + "new_" + file

with open(fpath) as f:
    source = f.read()
    f.close()

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


for func in func_names:
   fname = func
   print(fname)


print("scan ready")
