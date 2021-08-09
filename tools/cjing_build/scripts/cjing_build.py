import sys
import os.path
import json
import time
import shutil
import collections
import subprocess
import importlib
import platform
import utils
import jsc

# major version
major_version = 0
# features [0-100]
minor_version = 3
# patch update; bug fixes [0-1000]
match_version = 0

def get_version():
    return "v" + str(major_version) + "." + str(minor_version) + "." + str(match_version)

##########################################################################################
# Task Tools
##########################################################################################

def get_task_files_raw(files_task):
    files = []
    # 如果src_dir则获取目录下所有的文件路径
    if os.path.isdir(files_task[0]):
        dir_files = utils.walk_directory(files_task[0])
        if len(dir_files) > 0:
            for file in dir_files:
                files.append((
                    file, 
                    file.replace(utils.format_file_path(files_task[0]), utils.format_file_path(files_task[1]))
                ))
    # 如果是文件则直接添加目标文件
    else:
        files.append((
            utils.format_file_path(files_task[0]),
            utils.format_file_path(files_task[1])
        ))

    return files

def get_task_files(config, task_name):
    task_files = []
    task = config[task_name]
    if "files" not in task.keys():
        return task_files

    files = task["files"]
    for file_task in files:
        # 如果目标是字符串则改为[("", "")]形式，并交由后续处理
        if type(file_task) == str:
            path = file_task
            file_task = [path, ""]

        if type(file_task) == list:
            task_files.extend(get_task_files_raw(file_task))
        

    return task_files

##########################################################################################
# Task Methods
##########################################################################################

def task_run_tools(config, task_name, tool_name):
    if tool_name not in config["tools"].keys():
        return
    tool_exe = utils.format_file_path(config["tools"][tool_name])
    cmd = tool_exe
    if "args" in config[task_name].keys():
        cmd += " "
        for arg in config[task_name]["args"]:
            # check vs_version
            vs_str = "%{vs_version}"
            if arg.find(vs_str) != -1:
                arg = arg.replace(vs_str, config["user_vars"]["vs_version"]) 

            # check win sdk version
            wsv_str = "%{windows_sdk_version}"
            if arg.find(wsv_str) != -1:
                arg = arg.replace(wsv_str, config["user_vars"]["windows_sdk_version"]) 

            cmd += arg + " "

    child = subprocess.Popen(cmd, shell=True)
    e = child.wait()
    if e:
        print("[error] failed to run tool:" + cmd)
        exit(e)
    

def task_run_scripts(config, task_name, task_type, task_scritps):
    if "files" in config[task_name].keys():
        task_scritps.get(task_type)(config, task_name, get_task_files(config, task_name))
    else:
        task_scritps.get(task_type)(config, task_name, [])

def print_task_header(task_name):
    print("-----------------------------------------------------------------------")
    print("Task: -", task_name)
    print("-----------------------------------------------------------------------")

def taks_run_copy(config, task_name, files):
    if files == None or len(files) <= 0:
        return    
    for file_task in files:
        utils.copy_file_or_create_dir(file_task[0], file_task[1])

def taks_run_shell(config, task_name, files):   
    if "commands" not in config[task_name]:
        return
    commands = config[task_name]["commands"]
    if type(commands) != list:
        print("error: task shell's commands must be array of strings")
        exit(1)

    for command in commands:
        child = subprocess.Popen(command, shell=True)
        e = child.wait()
        if e:
            print("error: faild to run command:" + command)
            exit(1)
    
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

def task_run_build(config, task_name, files, args):
    if len(files) <= 0:
        print("error: no target to build")
        exit(1)

    if "build" not in config.keys():
        print("error: config.jsc miss the build config")
        exit(1)

    buildtool = config["build"]["buildtool"]
    # msbuild 需要预先设置环境
    if buildtool == "msbuild":
        set_env_cmd =  "pushd \ && cd /d \"" + config["user_vars"]["vcvarsall_dir"] + "\" && vcvarsall.bat x86_amd64 && popd"
        subprocess.call(set_env_cmd, shell=True)

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

    cwd = os.getcwd()
    for build_file in build_files:
        file_path = build_file[0]
        os.chdir(os.path.dirname(file_path))

        cmd = get_build_cmd(config, os.path.basename(file_path), args)
        p = subprocess.Popen(cmd, shell=True)
        e = p.wait()
        if e != 0:
            exit(1)
        os.chdir(cwd)

def task_run_clean(config, task_name):
    if "clean" not in config.keys():
        return
    print_task_header(task_name)

    clean_task = config[task_name]
    if "directories" in clean_task:
        for directory in clean_task["directories"]:
            print("clean directory:" + directory)
            shutil.rmtree(clean_task)

    if "files" in clean_task:
        for file in clean_task["files"]:
            print("clean file:" + file)
            os.remove(clean_task)

def task_run_shaders(config, task_name, args):
    if "shaders" not in config.keys():
        return
    print_task_header(task_name)

    # compile shader source
    if "shader_compiler" not in config["tools"].keys():
        return

    tool_exe = utils.format_file_path(config["tools"]["shader_compiler"])
    cmd = tool_exe
    if "args" in config[task_name].keys():
        cmd += " "
        for arg in config[task_name]["args"]:
            cmd += arg + " "
    if len(args) > 0:
        for arg in args:
            cmd += arg + " "
 
    child = subprocess.Popen(cmd, shell=True)
    e = child.wait()
    if e:
        print("[error] failed to compile shader:" + cmd)
        exit(e)

def task_run_generate_build_config(config, task_name, files):
    if "build_config" not in config.keys():
        return
    profile = config["user_vars"]["profile"]
    build_config = config[task_name]
    cwd = os.getcwd()
    dest_dir = build_config["dest"]

    data = {
        "profile" : profile,
        "build_cmd": utils.format_file_path(build_config["build_cmd"]),
        "build": "cd /d " + cwd + " && " + utils.format_file_path(build_config["build_cmd"]) + " "
    }

    # make dir
    dir = os.path.dirname(dest_dir)
    if not os.path.exists(dir):
        os.makedirs(dir)

    # generate config
    np = os.path.join(dest_dir, "build_config.json")
    np = os.path.normpath(np)
    file = open(np, "w+")
    file.write(json.dumps(data, indent=4))

    print("build config generated:", np)


##########################################################################################

def set_user_config_update(key, value, config):
    config[key] = value

    user_cfg = dict()
    user_cfg["user_vars"] = dict()
    if os.path.exists("config_user.jsc"):
        user_cfg["user_vars"] = json.loads(open("config_user.jsc", "r").read())["user_vars"]
    user_cfg["user_vars"][key] = value

    file = open("config_user.jsc", "w+")
    file.write(json.dumps(user_cfg, indent=4))
    file.close()

def set_user_config_vs_version(config):
    vs_version = utils.locate_laste_vs_version()
    if vs_version == "":
        print("error: count not find avaiable visual studio")
        exit(1)
    set_user_config_update("vs_version", vs_version, config)

def set_user_config_winsdk_version(config):
    if "windows_sdk_version" in config.keys():
        return

    sdk_version_list = utils.locate_winsdk_version_list()
    if sdk_version_list and len(sdk_version_list) > 0:
        sdk_version = sdk_version_list[0]
        if sdk_version:
            set_user_config_update("windows_sdk_version", sdk_version, config)
    else:
        print("error:Cannot find one setted Windows SDK")
        print("error:Some task is invalid")
        exit(1)

# find vcvarsall.bat directory
def set_user_config_vc_vars(config):
    if "vcvarsall_dir" in config.keys():
        if os.path.exists(config["vcvarsall_dir"]):
            return

    vcvarsall_dir = utils.locate_vcvarall()
    if vcvarsall_dir:
        vcvarsall_dir = os.path.dirname(vcvarsall_dir)
        set_user_config_update("vcvarsall_dir", vcvarsall_dir, config)
        return
    else:
        print("error:Cannot find vcvarsall.bat")
        print("error:Some task is invalid")
        exit(1)
    
# 构建user config，主要用于获取winsdk version和vs version
def set_user_config(config):
    user_cfg = dict()
    user_cfg["user_vars"] = dict()

    if os.path.exists("config_user.jsc"):
        user_cfg = json.loads(open("config_user.jsc", "r").read())

    if utils.get_platform_name() == "win32":
        set_user_config_vs_version(user_cfg["user_vars"])
        set_user_config_vc_vars(user_cfg["user_vars"])
        set_user_config_winsdk_version(user_cfg["user_vars"])

    # 需要将user_config合并到config中
    if os.path.exists("config_user.jsc"):
        user_cfg = json.loads(open("config_user.jsc", "r").read())
        utils.merge_dicts(config, user_cfg)


def print_help(config):
    print("-----------------------------------------------------------------------")
    print("Cjing Build system " + get_version())
    print("-----------------------------------------------------------------------")
    print("usage: pmbuild <options> <profile> <tasks...>")
    print("cmd arguments:")
    print("options:")
    print("      -help (display help)")
    print("      -show_cfg (print cfg json)")
    print("profile:(edit in config.jsc)")
    if config:
        for p in config.keys():
            print(" " * 6 + p)
    print("task:")
    print("      -all ")
    print("      -libs")
    print("      -premake")
    print("      -copy")

##########################################################################################
# Main Functions
##########################################################################################

def general_task_order(config_profile, is_all_task):
    task_order = []

    for task_name in config_profile.keys():
        task = config_profile[task_name]
        if type(task) != dict:
            continue

        if "type" not in task:
            continue

        # clean任务已经提前执行
        task_type_filter = ["clean", "datas", "tools", "none"]
        if task["type"] in task_type_filter:
            continue
        
        # 如果task["explicit"] = True，则需要在sys.argv显示传递命令参数
        if "explicit" in task.keys():
            if task["explicit"] == True and ("-" + task_name) not in sys.argv:
                continue

        # 如果sys.argv传递参数中显示禁止命令，则不执行
        if "-not" + task_name in sys.argv:
            continue
        if "-" + task_name in sys.argv or is_all_task:
            task_order.append(task_name)

    return task_order

def do_normal(profile_cfg, all_cfg, user_args):
    # task scirps
    task_scripts = {
        "copy"  : taks_run_copy,
        "shell" : taks_run_shell,
        "build" : task_run_build,
        "build_config" : task_run_generate_build_config,
    }

    # 执行extensions
    #   extensions: {
    #     texture: {
    #         search_path: "XXX/tools/ext"      #寻找的路径
    #         module: "texture_ext"             #使用的模块脚本(py)
    #         function: "run_texture"           #执行的函数
    #     }
    #  }
    if "extensions" in all_cfg.keys():
        for ext_name in all_cfg["extensions"].keys():
            ext = all_cfg["extensions"][ext_name]
            if "search_path" in ext.keys():
                sys.path.append(ext["search_path"])
            scripts = getattr(importlib.import_module(ext["module"]), ext["function"])

    # 给所有未设置type的task设置type
    for task_name in profile_cfg.keys():
        if task_name == "user_vars":
            continue

        task = profile_cfg[task_name]
        if type(task) == dict and "type" not in task.keys():
            profile_cfg[task_name]["type"] = task_name

    # 优先执行所有clean tasks
    if "-clean" in user_args:
        for name, task in profile_cfg:
            if "type" not in task:
                continue
            if task["type"] == "clean":
                task_run_clean(profile_cfg, name)

    # run tasks
    task_order = general_task_order(profile_cfg, "-all" in user_args)
    if len(task_order) == 0:
        exit(0)

    for task_name in task_order:
        task = profile_cfg[task_name]
        if type(task) != dict: 
            continue
        if "type" not in task:
            continue

        print_task_header(task_name)

        task_type = task["type"]
        if task_type in task_scripts.keys():
            task_run_scripts(profile_cfg, task_name, task_type, task_scripts)
        elif task_type in profile_cfg["tools"].keys():
            task_run_tools(profile_cfg, task_name, task_type)

def main():
    start_time = time.time()

    if not os.path.exists("config.jsc"):
        print("[error] no config.jsc in current directory.")
        exit(1)
    
    all_cfg = jsc.parse_jsc(open("config.jsc", "r").read())
    if not all_cfg:
        print("[error] invalid config:config.jsc.")
        exit(1)

    if len(sys.argv) <= 1:
        exit(0)

    build_mode = "normal"
    profile_index = 1
    if len(sys.argv) > 1 and sys.argv[1] == "build":
        profile_index = 2
        build_mode = "build"
    elif len(sys.argv) > 1 and sys.argv[1] == "shader":
        profile_index = 2
        build_mode = "shaders"

    # user pass args
    user_args = [
        "-help",
        "-clean",
        "-all"
    ]
    for arg in reversed(user_args):
        if arg not in sys.argv:
            user_args.remove(arg)
        else:
            sys.argv.remove(arg)

    # profile
    if profile_index < len(sys.argv):
        profile_name = sys.argv[profile_index]
        if profile_name not in all_cfg:
            print("[error] " + profile_name + " is not a valid profile")
            exit(0)
        profile_cfg = all_cfg[profile_name]
    else:
        profile_cfg = all_cfg
        
    # only display help
    if "-help" in user_args and len(sys.argv) == 1:
        print_help(all_cfg)
        exit(0)

    # print title header
    print("-----------------------------------------------------------------------")
    print("Cjing Build " + get_version())
    print("-----------------------------------------------------------------------")

    # set user config
    set_user_config(profile_cfg)
    if "user_vars" not in profile_cfg.keys():
        profile_cfg["user_vars"] = dict()
    if profile_index < len(sys.argv):
        profile_cfg["user_vars"]["profile"] = sys.argv[profile_index]

    # set tools
    profile_cfg["tools"] = dict()
    if "tools" in all_cfg.keys():
        profile_cfg["tools"] = all_cfg["tools"]

    # do main task
    if build_mode == "build":
        files = get_task_files(profile_cfg, "build")
        task_run_build(profile_cfg, "build", files, sys.argv[3:])
    elif build_mode == "shaders":
        task_run_shaders(profile_cfg, "shaders", sys.argv[3:])
    else:
        do_normal(profile_cfg, all_cfg, user_args)

    # finish jobs
    print("-----------------------------------------------------------------------")
    print("Finished. Took (" + str(int((time.time() - start_time) * 1000)) + "ms)")

if __name__ == '__main__':
    main()
