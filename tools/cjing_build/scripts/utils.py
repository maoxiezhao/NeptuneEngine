
import os
import platform
import glob
import shutil

#################################################################
## common utils
#################################################################

def merge_dicts(dst, src):
    for k, v in src.items():
        if type(v) == dict:
            if k not in dst:
                dst[k] = v
        else:
            dst[k] = v
         

#################################################################
## platform utils
#################################################################
support_platforms = {
    "Windows" : "win32",
    "Linux" : "linux",
}

def get_platform_name():
    platform_name = platform.system()
    return support_platforms[platform_name]

def locate_winsdk_version_list():
    winsdk_dir = ""
    winsdk_name = "Windows Kits"
    programfiles = [ "PROGRAMFILES", "PROGRAMFILES(X86)"]
    for programfile in programfiles:
        env_dir = os.environ[programfile]
        if env_dir:
            if winsdk_name in os.listdir(env_dir):
                winsdk_dir = os.path.join(env_dir, winsdk_name)
                break

    if winsdk_dir:
        for version in os.listdir(winsdk_dir):
            if version == "10":
                child_dir = os.path.join(winsdk_dir, version, "Source")
                if not os.path.exists(child_dir):
                    continue
                return sorted(os.listdir(child_dir), reverse=False)
    return None

def format_file_path(path):
    path = path.replace("/", os.sep)
    path = path.replace("\\", os.sep)
    return path

def walk_directory(dir_path):
    files_path_list = []
    for root, dirs, files in os.walk(dir_path):
        for file in files:
            files_path_list.append(format_file_path(os.path.join(root, dirs)))
    return files_path_list

def copy_file_or_create_dir(src_file, dst_file):
    if not os.path.exists(src_file):
        print("error: src file does not exists:" + src_file)
        return False
    try:
        dir = os.path.dirname(dst_file)
        if not os.path.exists(dir):
            os.makedirs(dir)

        src_file = os.path.normpath(src_file)
        dst_file = os.path.normpath(dst_file)
        print("copy " + src_file + " to " + dst_file)
        return True
    except Exception as e:
        print("error: failed to copy" + src_file)
        return False

#################################################################
## vs utils
#################################################################
support_vs_version = [
    "vs2019"
]

def locate_msbuild():
    vs_root_dir = locate_vs_root()
    if len(vs_root_dir) <= 0:
        return None

    # get latest msbuild.exe
    msbuild = sorted(glob.glob(os.path.join(vs_root_dir, "**/msbuild.exe"), recursive=True), reverse=False)
    if len(msbuild) > 0:
        return msbuild[0]
    return None

def check_vs_version(vs_version):
    if vs_version == "latest":
        return True
    return vs_version in support_vs_version

def locate_laste_vs_version():
    vs_root_dir = locate_vs_root()
    if len(vs_root_dir) <= 0:
        return ""

    ret = ""
    versions = sorted(os.listdir(vs_root_dir), reverse=False)
    for v in versions:
        vs_version = "vs" + v
        if vs_version in support_vs_version:
            ret = vs_version
    return ret

def locate_vs_root():
    vs_root = ""
    vs_directory_name = "Microsoft Visual Studio"
    programfiles = [ "PROGRAMFILES", "PROGRAMFILES(X86)"]
    for programfile in programfiles:
        env_dir = os.environ[programfile]
        if env_dir:
            if vs_directory_name in os.listdir(env_dir):
                vs_root = os.path.join(env_dir, vs_directory_name)
    return vs_root
    

def locate_vcvarall():
    vs_root_dir = locate_vs_root()
    if len(vs_root_dir) <= 0:
        return None
    
    # **匹配所有文件、目录
    # recursive=True 递归匹配
    ret = glob.glob(os.path.join(vs_root_dir, "**/vcvarsall.bat"), recursive=True)
    if len(ret) > 0:
        return ret[0]
    return None


if __name__ == '__main__':
    print("utils")