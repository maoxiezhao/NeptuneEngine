dofile("../tools/cjing_build/premake/options.lua")
dofile("../tools/cjing_build/premake/globals.lua")
dofile("../tools/cjing_build/premake/plugins.lua")
dofile("../tools/cjing_build/premake/example_app.lua")

app_name = "app"

start_project = app_name
if not build_editor then 
    start_project = app_name
end 

-- total solution
solution ("VulkanTest")
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

    create_example_app(
        "app",                          -- project_name
        "src",                          -- source_directory
        get_current_script_path(),      -- root_directory
        "ConsoleApp",                   -- app kind
        nil,                            -- plugins,
        default_engine_modules,         -- engine modules
        function(SOURCE_DIR)
            -- vulkan header
            includedirs { "../3rdparty/vulkan/include" }

            -- glfw
            includedirs { "../3rdparty/glfw/include" }
            libdirs {  "../3rdparty/glfw/lib/" .. platform_dir,  }
            filter {"configurations:Debug"}
                links {"glfw_lib_d"}
            filter {"configurations:Release"}
                links {"glfw_lib"}
            filter {}
        end
    )