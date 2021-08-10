local function setup_platform_win32()
    systemversion(windows_sdk_version())
end 

local function setup_platform()
    if platform_dir == "win32" then 
        setup_platform_win32()
    end 
end 

local function link_plugins(plugins, config)
    if plugins == nil then 
        return
    end 

    includedirs {env_dir .. "src/plugins/src"}

    for _, plugin in ipairs(plugins) do
        link_plugin(plugin)
    end
end 

----------------------------------------------------------------------------

function get_current_script_path()
    local str = debug.getinfo(2, "S").source:sub(2)
    return str:match("(.*/)")
end

function create_example_app(project_name, source_directory, root_directory, app_kind, plugins, ext_func)
    print("[APP]", project_name)

    local project_dir = root_directory .. "/build/" .. platform_dir .. "/" .. project_name
    local target_dir  = root_directory .. "/bin/" .. platform_dir
    local source_dir  = root_directory .. source_directory

    project (project_name)
        location(project_dir)
        objdir(project_dir .. "/temp")
        cppdialect "C++17"
        language "C++"
        conformanceMode(true)
        kind (app_kind)
        staticruntime "Off"
        setup_project_env()
        setup_platform()
        setup_project_definines()

        -- plugins
        if plugins ~= nil then 
            setup_plugins_definines(plugins)

            filter {"configurations:Debug"}
                link_plugins(plugins, "Debug")

            filter {"configurations:Release"}
                link_plugins(plugins, "Release")

            filter {}
        end 

        -- includes
        includedirs { 
            -- local
            source_dir,
            -- 3rdParty
            env_dir .. "3rdparty/",       
            -- shaderInterop
            env_dir .. "assets/shaders", 
        }

        -- Files
        files 
        { 
            source_dir .. "/**.c",
            source_dir .. "/**.cpp",
            source_dir .. "/**.hpp",
            source_dir .. "/**.h",
            source_dir .. "/**.inl"
        }

        -- Debug dir
        local debug_dir = ""
        if work_dir ~= nil then 
            debug_dir = work_dir
        else
            debug_dir = env_dir
        end 
        debugdir (debug_dir)

        if ext_func ~= nil then 
            ext_func(source_dir)
        end 

        --------------------------------------------------------------
        -- Config
        targetdir (target_dir)
        -- Debug config
        filter {"configurations:Debug"}
            targetname(project_name)
            defines { "DEBUG" }

        -- Release config
        filter {"configurations:Release"}
            targetname(project_name .. "_d")
            defines { "NDEBUG" }

        filter { }
        --------------------------------------------------------------
end