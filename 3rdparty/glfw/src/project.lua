
local GLFW_SOURCE_DIR = "..\\";
-- Project	
project "glfw"
	location (GLFW_SOURCE_DIR .. "build\\" .. platform_dir)
	kind "StaticLib"
	objdir(GLFW_SOURCE_DIR .. "build\\" .. platform_dir .. "\\Temp")
	language "C++"

	if _ACTION == "vs2017" or _ACTION == "vs2015" or ACTION == "vs2019" then
		systemversion(windows_sdk_version())
	end

	-- includedirs
	includedirs { 
        GLFW_SOURCE_DIR .. "include"
    }
			
	-- Files
	files 
	{ 
        "internal.h",
        "mappings.h",
        GLFW_SOURCE_DIR .. "src/glfw_config.h",
        GLFW_SOURCE_DIR .. "include/GLFW/glfw3.h",
        GLFW_SOURCE_DIR .. "include/GLFW/glfw3native.h",
        "context.c",
		"init.c",
		"input.c",
		"monitor.c",
        "vulkan.c",
        "window.c"
	}
    
    -- win32
    if platform_dir == "win32" then 
        defines 
		{ 
			"_GLFW_WIN32",
			"_CRT_SECURE_NO_WARNINGS"
		}

        files
		{
			"win32_init.c",
			"win32_joystick.c",
			"win32_monitor.c",
			"win32_time.c",
			"win32_thread.c",
			"win32_window.c",
			"wgl_context.c",
			"egl_context.c",
			"osmesa_context.c"
		}
    end
				
    filter {"configurations:Debug"}
		defines { "DEBUG" }
		entrypoint "WinMainCRTStartup"
		symbols "On"
		targetdir (GLFW_SOURCE_DIR .. "lib/" .. platform_dir)
		targetname "glfw_lib_d"
 
	filter {"configurations:Release"}
		defines { "NDEBUG" }
		entrypoint "WinMainCRTStartup"
		optimize "Speed"
		targetdir (GLFW_SOURCE_DIR .. "lib/" .. platform_dir)
		targetname "glfw_lib"


