dofile("../tools/cjing_build/premake/options.lua")
dofile("../tools/cjing_build/premake/globals.lua")
dofile("../tools/cjing_build/premake/plugins.lua")
dofile("../tools/cjing_build/premake/example_app.lua")

app_name = "app"

start_project = app_name
if not build_editor then 
    start_project = app_name
end 

if sln_name == "" then 
    sln_name = "Test"
end 

-- total solution
solution (sln_name)
    location ("build/" .. platform_dir ) 
    cppdialect "C++17"
    language "C++"
    startproject "test"
    configurations { "Debug", "Release" }
    setup_project_env()

    -- Debug config
    filter {"configurations:Debug"}
        flags { "MultiProcessorCompile"}
        symbols "On"

    -- Release config
    filter {"configurations:Release"}
        flags { "MultiProcessorCompile"}
        optimize "On"

    -- Reset the filter for other settings
    filter { }
    
    dofile "../modules/modules.lua"
    -- dofile "../tests/project.lua"

    create_example_app(
        "app",                          -- project_name
        "src",                          -- source_directory
        get_current_script_path(),      -- target_directory
        "ConsoleApp",                   -- app kind
        nil,                            -- plugins,
        default_engine_modules,         -- engine modules
        function(SOURCE_DIR)
            -- vulkan header
            includedirs { "../3rdparty/vulkan/include" }
            includedirs { "../assets/shaders/common" }
        end
    )