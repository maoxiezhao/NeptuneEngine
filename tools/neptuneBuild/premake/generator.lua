
local function get_setup_header_tools_dir()
    return path.getabsolute("../" .. header_tools_dir .. "/bin/x64/Debug/headerTools.exe")
end 

function setup_header_tools(module_name)
    local tools_dir = get_setup_header_tools_dir()
    local target_dir = path.getabsolute(module_location .. "/" .. module_name)
    prebuildcommands {
        tools_dir .. " -d " .. target_dir
    }
end 