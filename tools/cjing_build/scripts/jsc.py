
# jsc 是一种基于json的文件格式，在json基础上拓展了一些新的特性支持
# 该模块会将jsc解析成json格式

import json
import sys
import os
import traceback
import platform

# 目前支持的platforms
support_platforms = [
    "Windows",
    "Linux"
]

#########################################################################################################
# utils
#########################################################################################################

def change_path_extension(dir, ext):
    return os.path.splitext(dir)[0] + ext

def check_path_directory(dir):
    return len(os.path.splitext(dir)[1]) <= 0

def put_stirng_in_quotes(string):
    if len(string) > 0:
        if string[0] == "\"":
            return string
    return "\"" + string + "\""

def get_value_type(value):
    value = value.strip()
    if len(value) == 0:
        return "string"

    if value[0] == "\"":
        return "string"
    if value[0] == "{":
        return "object"
    if value[0] == "[":
        return "array"
    if value == 'true' or value == 'false':
        return "bool"
    if value.find(".") != -1:
        try:
            float(value)
            return "float"
        except ValueError:
            pass
    if value.find("0x") != -1:
        try:
            int(value[2:], 16)
            return "hex"
        except ValueError:
            pass
    if value.find("0b") != -1:
        try:
            int(value[2:], 2)
            return "binary"
        except ValueError:
            pass
    try:
        int(value)
        return "int"
    except ValueError:
        return "string"
    
def to_uint(v):
    if v < 0:
        return sys.maxsize
    return v

def find_chars_first(string, pos, chars):
    ret = to_uint(-1)
    for char in chars:
        ret = min(ret, to_uint(string.find(char, pos)))
    return ret

def get_closed_brackets_end_pos(open, close, string, start_pos):
    start_pos = string.find(open, start_pos)
    if start_pos == -1:
        return -1

    start_pos += 1
    stack = [open]
    while len(stack) > 0 and start_pos < len(string):
        char = string[start_pos]
        if char == open:
            stack.append(open)
        elif char == close:
            stack.pop()
        start_pos += 1
    return start_pos

#########################################################################################################
# object
#########################################################################################################

def quoted_value(value, pos, next_pos):
    quoted = ""
    value_type = get_value_type(value)
    if value_type == "string":
        quoted += put_stirng_in_quotes(value)
        pos = next_pos
    elif value_type == "hex":
        hex_value = int(value[2:], 16)
        quoted = str(hex_value)
        pos = next_pos
    elif value_type == "binary":
        binary_value = int(value[2:], 2)
        quoted = str(binary_value)
        pos = next_pos
    elif value_type == "float":
        quoted = value
        pos = next_pos
    elif value_type == "int":
        quoted = value
        pos = next_pos
    return (quoted, pos)

def quoted_array(array_string):
    ret = ""
    pos = 0
    while True:
        item_end = array_string.find(",", pos)
        if item_end == -1:
            item_end = len(array_string)
        
        item = array_string[pos:item_end].strip()
        if len(item) == 0:
            break

        item_type = get_value_type(item)
        if item_type == "object":
            # {}中可能存在",", 重新定位item_end
            item_end = get_closed_brackets_end_pos("{", "}", array_string, pos)
            if item_end == -1:
                break
            ret += quoted_object(array_string[pos+1:item_end-1])

        elif item_type == "array":
            # []中可能存在",", 重新定位item_end
            item_end = get_closed_brackets_end_pos("[", "]", array_string, pos)
            if item_end == -1:
                break
            ret += quoted_array(array_string[pos+1:item_end-1])
        elif item[0] == "\"":
            # 字符串中可能有",", 重新定位item_end
            item_end = get_closed_brackets_end_pos("\"", "\"", array_string, pos)
            if item_end == -1:
                break
            ret += quoted_value(array_string[pos:item_end], 0, 0)[0]
        else:
            ret += quoted_value(item, 0, 0)[0]

        pos = item_end + 1
        if pos >= len(array_string):
            break
        ret += ","

    return "[" + ret + "]"

def quoted_object_key(key, value, pos):
    quoted = ""
    if get_value_type(value) != "object":
        return quoted

    quoted += "{"
    
    # 获取object inherit list
    inherits = []
    bp = key.find("(")
    if bp != -1:
        ep = key.find(")")
        inherit_str = key[bp+1:ep]
        inherits = inherit_str.split(",")
        key = key[:bp]

    if len(inherits) > 0:
        quoted += "\"jsc_inherit\":["
        for i in range(0, len(inherits)):
            if i > 0:
                quoted += ", "
            quoted += put_stirng_in_quotes(inherits[i])
        quoted += "],"

    return key, quoted

def quoted_object(jsc):
    delimiters = [",", "{"]

    ret = ""
    pos = 0
    string_list = jsc_get_string_index_list(jsc)
    while True:
        cur_pos = pos
        pos = jsc.find(":", pos)
        if pos == -1:
            ret += jsc[cur_pos:]
            break

        # ignore inside quote
        in_string = jsc_check_in_string_index_list(string_list, pos)
        if in_string:
            ret += jsc[cur_pos:in_string]
            pos = in_string
            continue

        # get key
        key_start = 0
        for d in delimiters:
            dd = jsc[:pos].rfind(d)
            if dd != -1:
                key_start = max(key_start, dd)
        key = jsc[key_start+1:pos].strip()

        # 如果key_start在（）中，则重新寻找key_start
        if key.find(")") != -1:
            sp = jsc[:pos].rfind("(")
            ep = jsc.find(")", key_start)
            if sp < key_start < ep:
                key_start = 0
                for d in delimiters:
                    dd = jsc[:sp].rfind(d)
                    if dd != -1:
                        key_start = max(key_start, dd)
                key = jsc[key_start+1:pos].strip()

        # get first value pos
        pos += 1 # skip ":"
        value_end_pos = find_chars_first(jsc, pos, [",", "}", "]"])
        while jsc_check_in_string_index_list(string_list, value_end_pos):
                value_end_pos = find_chars_first(jsc, value_end_pos + 1, [",", "}", "]"])
        value = jsc[pos:value_end_pos]

        # 带有继承的object会影响key，需要先特殊处理
        object_inherits = ""
        if get_value_type(value) == "object":
            key, object_inherits = quoted_object_key(key, value, pos)
            pos += 1

        # put key in quotes
        ret += jsc[cur_pos:key_start+1]
        ret += put_stirng_in_quotes(key)
        ret += ":"
        ret += object_inherits

        # put value in quotes and connect value
        if get_value_type(value) == "array":
            end = get_closed_brackets_end_pos("[", "]", jsc, pos)
            if end == -1:
                print("error: invalid array")
                exit(1)
            ret += quoted_array(jsc[pos + 1:end - 1])
            pos = end
        else:
            info = quoted_value(value, pos, value_end_pos)
            ret += info[0]
            pos = info[1]

    return ret

def process_inherit(child, parent):
    for k, v in parent.items():
        if type(v) != dict:
            if not k in child:
                child[k] = v
        else:
            # 如果child属性不是dict，则以child为主
            if k in child and type(child[k]) != dict:
                continue

            if k not in child:
                child[k] = dict()
            process_inherit(child[k], v)
            

def process_inherit_recursive(jsn, jsn_parent):
    inherits = []
    for k, v in jsn.items():
        if k == "jsc_inherit":
            for i in v:
                inherits.append(i)
    
    if len(inherits) > 0:
        jsn.pop("jsc_inherit", None)
        for inherit in inherits:
            if inherit in jsn_parent.keys():
                process_inherit(jsn, jsn_parent[inherit])


    for k, v in jsn.items():
        if type(v) == dict:
            process_inherit_recursive(v, jsn)

def process_variable(var, def_vars):
    variables = []

    # get variables
    string = str(var)
    pos = 0
    while True:
        sp = string.find("${", pos)
        if sp == -1:
            break
        else:
            ep = string.find("}", sp)
            variables.append(string[sp:ep+1])
            pos = sp + 2

    # get variables from def_vars
    for variable_index in range(0, len(variables)):
        variable = variables[variable_index]
        variable_name = variable[2:len(variable)-1]

        if variable_name in def_vars.keys():
            # type only is list or name
            if type(var) == list:
                for i in range(0, len(var)):
                    list_v = var[i]
                    r_list_v = process_variable(list_v, def_vars)
                    if r_list_v:
                        var[i] = r_list_v
                return var
            else:
                if type(def_vars[variable_name]) == str:
                    var = var.replace(variable, str(def_vars[variable_name]))
                    if variable_index == len(variables) - 1:
                        return var
                else:
                    return def_vars[variable_name]

        else:
            print("error: undefined variable:" + variable_name)
            print("current def vars:")
            print(json.dumps(def_vars, indent=4))
            exit(1)
        count += 1
    return None

def process_variables_recursive(jsn, def_vars):
    # 避免引用传递，放置下一层的变量声明影响这一层
    current_def_vars = def_vars.copy() 

    def_key_word = "jcs_def"
    if def_key_word in jsn.keys():
        for k, v in jsn[def_key_word].items():
            current_def_vars[k] = v

    # 递归遍历所有的k,v
    for k, v in jsn.items():
        if type(v) == dict:
            process_variables_recursive(v, current_def_vars)
        elif type(v) == list:
            for i in range(0, len(v)):
                list_v = v[i]
                r_list_v = process_variable(list_v, current_def_vars)
                if r_list_v:
                    v[i] = r_list_v
        else:
            var = process_variable(v, current_def_vars)
            if var:
                jsn[k] = var

    if def_key_word in jsn.keys():
        jsn.pop(def_key_word, None)


def process_platform_keys_recursive(jsn, platform_name):
    to_removed_keys = []
    platform_dict = dict()
    for key in jsn.keys():
        value = jsn[key]
        bp = key.find("<")
        ep = key.find(">")
        if bp != -1 and ep != -1:
            key_platform = key[bp + 1:ep]
            real_key = key[:bp]

            if key_platform == platform_name:
                platform_dict[real_key] = value
            to_removed_keys.append(key)
        
        if type(value) == dict:
            process_platform_keys_recursive(value, platform_name)

    # 将移除platform的key重新写入当前jsn
    for key in to_removed_keys:
        jsn.pop(key)

    if len(platform_dict) > 0:
        process_inherit(jsn, platform_dict)
   

def process_platform_keys(jsn):
    platform_name = platform.system()
    if platform_name in support_platforms:
        process_platform_keys_recursive(jsn, platform_name)
    else:
        print("warning: unknown platform systme:" + platform_name)
        platform_name = "Unknown"


#########################################################################################################
# # jsc parser
#########################################################################################################

def jsc_get_string_index_list(string):
    prev_c = ""
    quote = ""
    index_start = 0

    str_list = []
    for ic in range(0, len(string)):
        c = string[ic]
        if c == "'"  or  c == "\"":
            if quote == "":
                quote = c
                index_start = ic
            
            # \" 和 \' 视为字符串
            elif quote == c and prev_c != "\\":
                quote = ""
                str_list.append((index_start, ic))

        # \\ 不视为转义符
        if prev_c == "\\" and c == "\\":
            prev_c = ""
        else:
            prev_c = c
    return str_list

def jsc_check_in_string_index_list(string_list, index):
    for index_slice in string_list:
        if index <= index_slice[0]:
            break
        elif index < index_slice[1]:
            return index_slice[1] + 1
    return 0
    

def jsc_remove_comments(jsc):
    ret = ""
    lines = jsc.split("\n")
    in_comment = False
    for line in lines:
        string_list = jsc_get_string_index_list(line)

        # 检查/**/注释
        if in_comment:
            spos_end = line.find("*/")
            if spos_end != -1:
                line = line[:spos_end + 2]
                in_comment = False
            else:
                continue
            
        cpos = line.find("//")
        spos = line.find("/*")

        # 如果注释符号在字符串中则不考虑
        if jsc_check_in_string_index_list(string_list, cpos):
            cpos = -1
        if jsc_check_in_string_index_list(string_list, spos):
            spos = -1

        if cpos != -1:
            ret += line[:cpos] + "\n"
        elif spos != -1:
            ret += line[:spos] + "\n"
            spos_end = line.find("*/")
            if spos_end == -1 or spos_end < spos:
                in_comment = True
        else:
            ret += line + "\n"
    return ret

def jsc_change_quotes(jsc):
    ret = ""
    string_list = jsc_get_string_index_list(jsc)
    for ic in range(0, len(jsc)):
        c = jsc[ic]
        if c == "'":
            if not jsc_check_in_string_index_list(string_list, ic):
                ret += "\""
                continue
        ret += c
    return ret

def jsc_clean_src(jsc):
    ret = ""   
    in_string = False
    for c in jsc:
        if c == "\"":
            in_string = not in_string
        
        if not in_string:
            new_c = c.strip()
        else:
            new_c = c
        ret += new_c

    return ret

def jsc_get_imports(jsc, import_dir):
    imports = []

    bp = jsc.find("{")
    heads = jsc[:bp].split("\n")
    has_imports = False
    for head in heads:
        if head.find("import") != -1:
            has_imports = True
            break
    if not has_imports:
        return jsc, imports 

    if not import_dir:
        import_dir = os.getcwd()
    for head in heads:
        if head.find("import") != -1:
            import_file = head[len("import"):].strip().strip("\"")
            import_file = os.path.join(import_dir, import_file)
            if os.path.exists(import_file):
                imports.append(import_file)
            else:
                print("ERROR: failed to import file " + import_file)

    jsc = jsc[bp:]   
    return jsc, imports

def jsc_remove_trail_comma(jsc):
    ret = ""
    for ic in range(0, len(jsc)):
        c = jsc[ic]
        if ic < len(jsc) - 1:        
            next_c = jsc[ic + 1] 
            if c == "," and (next_c == "}" or next_c == "]"):
                continue
        ret += c
    return ret

#########################################################################################################

def jsc_parse_jsc(jsc):
    # json 不支持注释，首先移除注释
    jsc = jsc_remove_comments(jsc)
    # 将'装换成"
    jsc = jsc_change_quotes(jsc)
    # strip src
    jsc = jsc_clean_src(jsc)
    # 给unquoted keys添加quotes
    jsc = quoted_object(jsc)
    # 移除trail comma
    jsc = jsc_remove_trail_comma(jsc)

    return jsc

class ArgsInfo:
    input_files = []
    output_dir = ""
    import_dir = ""

def PrintHelp():
    print("usage: [cmd] [params]")
    print("cmd arguments:")
    print("-f   : list of input files")
    print("-o   : ouput directory")
    print("-i   : import directory")

def parse_jsc(jsc, import_dir=None, parse_var=True):
    # get imports
    jsc, imports = jsc_get_imports(jsc, import_dir)

    # 首先处理jsc buffer
    jsc = jsc_parse_jsc(jsc)
    
    # 根据处理后的jsc buffer读取json
    try:
        jsn = json.loads(jsc)
    except:
        traceback.print_exc()
        exit(1)

    # 加载import项
    for import_file in imports:
        import_jsn = load_jsc(import_file, import_dir, False)
        if import_jsn:
            process_inherit(jsn, import_jsn) 

    # 处理platform keys
    process_platform_keys(jsn)

    # 处理inherit
    process_inherit_recursive(jsn, jsn)

    # 处理variables
    if parse_var:
        process_variables_recursive(jsn, dict())
    
    return jsn

def load_jsc(file_name, import_dir=None, parse_var=True):
    if not file_name:
        return
    jsc = open(file_name).read()
    if not jsc:
        return
    return parse_jsc(jsc, import_dir, parse_var)

def parse_args():
    args_info = ArgsInfo()
    if len(sys.argv) == 1:
        PrintHelp()
        return args_info
    
    for i in range(1, len(sys.argv)):
        if sys.argv[i] == "-f":
            file_index = i + 1
            while file_index < len(sys.argv) and sys.argv[file_index][0] != "-":
                args_info.input_files.append(sys.argv[file_index])
                file_index = file_index + 1
            i = file_index

        elif sys.argv[i] == "-o":
            args_info.output_dir = sys.argv[i + 1]

        elif sys.argv[i] == "-i":
            args_info.import_dir = sys.argv[i + 1]

    return args_info

def generate(args_info, input_file_name, output_file_name):
    print("start to generate:", input_file_name, output_file_name)
    out_file = open(output_file_name, "w+")
    json_obj = load_jsc(input_file_name, args_info.import_dir, True)
    if json_obj:
        out_file.write(json.dumps(json_obj, indent=4))
    out_file.close()
    print("finish generating:", input_file_name)

def main():
    args_info = parse_args()

    output_dir = args_info.output_dir
    if not output_dir or not check_path_directory(output_dir) or len(args_info.input_files) == 0:
        exit(1)

    for i in args_info.input_files:
        # 如果path未创建，则创建dir
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        output_file_name = os.path.join(output_dir, i)
        output_file_name = change_path_extension(output_file_name, ".json")

        generate(args_info, i, output_file_name)

if __name__ == '__main__':
    main()