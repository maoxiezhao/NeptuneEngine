import sys
import os.path
import subprocess

##########################################################################################
# definition
##########################################################################################
_3rdLibs = [
    "glfw"
]

##########################################################################################
# functions
##########################################################################################
def generate_project(lib_name):
    global current_platform
    lib_name = lib_name + current_platform
    cmd = "..\\build.cmd " + lib_name + " -premake"
    child = subprocess.Popen(cmd, shell=True)
    e = child.wait()
    if e:
        print("error: failed to generate 3rdparty project:" + cmd)
        exit(e)

def build_project(lib_name, configuration):
    global current_platform
    lib_name = lib_name + current_platform
    cmd = "..\\build.cmd build " + lib_name + " all"

    # win32
    if current_platform == "-win32":
        cmd += " /p:Platform=x64"
        cmd += " /p:Configuration=" + configuration

    child = subprocess.Popen(cmd, shell=True)
    e = child.wait()
    if e:
        print("error: failed to build 3rdparty project:" + cmd)
        exit(e)

def build_lib(lib_name):
    print("-----------------------------------------------------------------------")
    print("[third party] " + lib_name)
    print("-----------------------------------------------------------------------")
    generate_project(lib_name)
    build_project(lib_name, "Debug")
    build_project(lib_name, "Release")

def main():
    if len(sys.argv) < 2:
        print("error: build 3rd libs must pass platform param")
        exit(0)
        
    global current_platform
    current_platform = sys.argv[1]

    for lib_name in _3rdLibs:
        build_lib(lib_name)

if __name__ == '__main__':
    main()
