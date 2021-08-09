
local function setup_platform_win32()
    systemversion(windows_sdk_version())
end 

local function setup_platform()
    if platform_dir == "win32" then 
        setup_platform_win32()
    end 
end 

local function set_module_env(module_dependencies, config)
    if not module_dependencies then 
        return
    end 
    
    local dependency_map = {}
    
    for _, dependency in ipairs(module_dependencies) do 
        get_all_dependencies(dependency, dependency_map)
    end 

    for dependency, v in pairs(dependency_map) do 
        libdirs {"../" .. dependency .. "/lib/" .. config}
        links {dependency}
        includedirs {"../" .. dependency .. "/src"}
    end 
end 

local function set_plugin_env(plugin_dependencies, config)
    if not plugin_dependencies then 
        return
    end 
    
    -- TODO:
    -- need to get dependencies from plugin

    -- local dependency_map = {}
    -- for _, dependency in ipairs(module_dependencies) do 
    --     get_all_dependencies(dependency, dependency_map)
    -- end 

    for _, dependency in ipairs(plugin_dependencies) do 
        links {dependency}
    end
end 

function setup_plugins_definines(plugins)
    if #plugins <= 0 then 
        return
    end 

    local definines_str = ""
    for index, plugin in ipairs(plugins) do
        if index > 1 then 
            definines_str = definines_str .. ",";
        end
        definines_str = definines_str .. "\"" .. plugin .. "\""
    end
    defines { "CJING_PLUGINS=" .. definines_str }
end 


function link_plugin(plugin_name)
    links(plugin_name)
    if is_static_plugin then 
        force_Link ("static_plugin_" .. plugin_name)
    end 
end 

function create_plugin(plugin_name, module_dependencies, plugin_dependencies, ext_func)
    print("[Plugin]", plugin_name)
    table.insert(all_plugins, plugin_name)

    project (plugin_name)
    location("build/" ..  platform_dir)
    objdir("build/" ..  platform_dir .. "/" .. plugin_name .. "/temp")
    kind "StaticLib"
    language "C++"
    conformanceMode(true)
    setup_project_env()
    setup_platform()
    setup_project_definines()
    targetname(plugin_name)

    -- Files
    local SOURCE_DIR = "src/" .. plugin_name
	files 
	{ 
        SOURCE_DIR .. "/**.c",
		SOURCE_DIR .. "/**.cpp",
        SOURCE_DIR .. "/**.hpp",
        SOURCE_DIR .. "/**.h",
        SOURCE_DIR .. "/**.inl",
    }
    
    -- includes
    includedirs {
        -- local
        SOURCE_DIR,
        "src",
        -- 3rdParty
        "../../3rdparty", 
    }

    -- Debug config
    filter {"configurations:Debug"}
        targetdir ("lib/" .. platform_dir .. "/Debug")
        defines { "DEBUG" }
        libdirs {"lib/" .. platform_dir .. "/Debug"}
        set_module_env(module_dependencies, "Debug")
        set_plugin_env(plugin_dependencies, "Debug")

    -- Release config
    filter {"configurations:Release"}
        targetdir ("lib/" .. platform_dir .. "/Release")
        defines { "NDEBUG" }
        set_module_env(module_dependencies, "Release")
        set_plugin_env(plugin_dependencies, "Release")
    filter { }

    -- do ext function
    if ext_func ~= nil then 
        ext_func(SOURCE_DIR)
    end 
end