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
        "ConsoleApp",
        nil,
        function(SOURCE_DIR)
            -- vulkan
            includedirs { "../3rdparty/vulkan/include" }
            local vulkan_source_dir = "../3rdparty/vulkan/include/vulkan"
            files 
            {
                vulkan_source_dir .. "/**.c",
                vulkan_source_dir .. "/**.cpp",
                vulkan_source_dir .. "/**.hpp",
                vulkan_source_dir .. "/**.h",
                vulkan_source_dir .. "/**.inl"
            }
            vpaths { 
                [""] =  {
                    SOURCE_DIR .. "/**.c",
                    SOURCE_DIR .. "/**.cpp",
                    SOURCE_DIR .. "/**.hpp",
                    SOURCE_DIR .. "/**.h",
                    SOURCE_DIR .. "/**.inl",
                },
                ["vulkan"] =  {
                    vulkan_source_dir .. "/**.c",
                    vulkan_source_dir .. "/**.cpp",
                    vulkan_source_dir .. "/**.hpp",
                    vulkan_source_dir .. "/**.h",
                    vulkan_source_dir .. "/**.inl"
                }
            }

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