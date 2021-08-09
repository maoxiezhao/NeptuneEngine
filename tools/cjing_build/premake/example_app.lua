local function setup_platform_win32()
    systemversion(windows_sdk_version())
end 

local function setup_platform()
    if platform_dir == "win32" then 
        setup_platform_win32()
    end 
end 

local function setup_third_modules()
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

local function link_all_extra_dependencies(dependencies, config)
    if dependencies == nil or type(dependencies) ~= "table" then 
        return
    end 
    
    for _, dependency in ipairs(dependencies) do 
        libdirs {"../" .. dependency .. "/lib/" .. config}
        links {dependency}     
        setup_dependent_libs(dependency, "config")
    end 
end 

----------------------------------------------------------------------------

function get_current_script_path()
    local str = debug.getinfo(2, "S").source:sub(2)
    return str:match("(.*/)")
end

function create_example_app(project_name, source_directory, root_directory, app_kind, plugins, extra_dependencies, ext_func)
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

        -- 3rd library
        setup_third_modules()

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
            debug_dir = env_dir .. "assets"
        end 
        debugdir (debug_dir)

        if ext_func ~= nil then 
            ext_func()
        end 

        -- set extra dependencies depenson
        if extra_dependencies ~= nil and type(extra_dependencies) == "table" then 
            for _, dependency in ipairs(extra_dependencies) do 
                includedirs { env_dir .. "src/" .. dependency .. "/src" }
                dependson { dependency }
            end 
        end 

        -- set engine dependencies
        local engine_dependencies = default_engine_modules

        --------------------------------------------------------------
        -- Config
        targetdir (target_dir)
        -- Debug config
        filter {"configurations:Debug"}
            targetname(project_name)
            defines { "DEBUG" }
            setup_engine("Debug", engine_dependencies)
            link_all_extra_dependencies(extra_dependencies, "Debug")

        -- Release config
        filter {"configurations:Release"}
            targetname(project_name .. "_d")
            defines { "NDEBUG" }
            setup_engine("Release", engine_dependencies)
            link_all_extra_dependencies(extra_dependencies, "Release")

        filter { }
        --------------------------------------------------------------
end