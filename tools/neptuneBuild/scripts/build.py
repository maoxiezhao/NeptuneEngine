import os
from platform import platform
import subprocess
import utils
import config

# Build target
class BuildTarget:
    name = ""
    project_name = ""

def get_build_cmd(config, file_name, args):
    make_config = config["build"]
    buildtool = make_config["buildtool"]

    extra_args = ""
    for arg in args:
        if arg == "all":
            continue
        extra_args += arg + " "

    build_cmd = ""

    # msbuild
    if buildtool == "msbuild":
        msbuild = utils.locate_msbuild()
        if not msbuild:
            msbuild = "msbuild"
        else:
            msbuild = "\"" + msbuild + "\""
        build_cmd = msbuild + " " + file_name + " " + extra_args

    return build_cmd

def build_targets_msbuild(config, build_files, args):
    # 1. msbuild 需要预先设置环境
    set_env_cmd =  "pushd \ && cd /d \"" + config["user_vars"]["vcvarsall_dir"] + "\" && vcvarsall.bat x86_amd64 && popd"
    subprocess.call(set_env_cmd, shell=True)

    cwd = os.getcwd()
    for build_file in build_files:
        file_path = build_file[0]
        os.chdir(os.path.dirname(file_path))

        cmd = get_build_cmd(config, os.path.basename(file_path), args)
        print(cmd)
        # p = subprocess.Popen(cmd, shell=True)
        # e = p.wait()
        # if e != 0:
        #     exit(1)
        # os.chdir(cwd)

def build_targets(config, files, args):
    build_files = []
    for file in files:
        filename = os.path.splitext(os.path.basename(file[0]))[0]
        if args[0] == "all":
            pass
        elif args[0] != filename:
            continue
        build_files.append(file)

    if len(build_files) <= 0:
        print("error: no target to build")
        exit(1)

    buildtool = config["build"]["buildtool"]
    if buildtool == "msbuild":
        build_targets_msbuild(config, build_files, args)