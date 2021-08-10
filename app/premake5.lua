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
    
    create_example_app(
        "app", 
        "src", 
        get_current_script_path(), 
        "ConsoleApp"
    )