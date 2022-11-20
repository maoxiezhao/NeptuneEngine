-----------------------------------------------------------------------------------
-- custom api
-----------------------------------------------------------------------------------
require('vstudio')

dofile("class.lua")
dofile("modules.lua")
dofile("platform.lua")
dofile("generator.lua")

premake.api.register {
    name = "conformanceMode",
    scope = "config",
    kind = "boolean",
    default = true
}

local function HandleConformanceMode(cfg)
    if cfg.conformanceMode ~= nil and cfg.conformanceMode == true then
        premake.w('<ConformanceMode>true</ConformanceMode>')
    end
end
  
if premake.vstudio ~= nil then 
    if premake.vstudio.vc2010 ~= nil then 
        premake.override(premake.vstudio.vc2010.elements, "clCompile", function(base, cfg)
            local calls = base(cfg)
            table.insert(calls, HandleConformanceMode)
            return calls
        end)
    end 
end 
-----------------------------------------------------------------------------------

function windows_sdk_version()
	if sdk_version ~= "" then
		return sdk_version
	end
	return "10.0.18362.0"
end

-----------------------------------------------------------------------------------
-- env
-----------------------------------------------------------------------------------
sln_name = ""
renderer = ""
platform_dir = ""
sdk_version = ""
env_dir = "../"
net_lib = ""
current_platform = "unknown"
is_static_plugin = true
renderdoc_debug = true
work_dir = nil
project_dir = nil
build_editor = true
build_app = false
build_tests = false
all_plugins = {}
target_architecture = ""
test_mode = true
with_shader_compiler = true
header_tools_dir = ""

function setup_project_env_win32()
    platforms "x64"
    filter { "platforms:x64" }
        system "Windows"
        architecture "x64"

    defines { "WIN32" }
    defines { "WIN64" }
end 

function setup_project_env_linux()
	print("Linux is unspported now.")
end 

function setup_project_env()
    if current_platform == "win32" then 
        setup_project_env_win32()
    elseif current_platform == "linux" then 
        setup_project_env_linux()
    end

	configuration "Debug"
		symbols "On"

    configuration {}
        defines { "_ITERATOR_DEBUG_LEVEL=0", "STBI_NO_STDIO" }
end

function setup_project_definines()
    defines
    {
        ("CJING3D_PLATFORM_" .. string.upper(platform_dir)),
        ("CJING3D_RENDERER_" .. string.upper(renderer)),
        ("NOMINMAX")
    }

    if net_lib ~= "" then 
        defines { "CJING3D_NETWORK_" .. string.upper(net_lib) }
    end 

    if is_static_plugin then 
        defines { "STATIC_PLUGINS" }
    end 

    if renderdoc_debug then 
        defines { "DEBUG_RENDERDOC" }
    end 

    if build_editor then 
        defines { "CJING3D_EDITOR" }
    end

    if test_mode then 
        defines { "CJING3D_TESTS" }
    end

    if with_shader_compiler then 
        defines { "COMPILE_WITH_SHADER_COMPILER" }
    end 
end 

function force_Link(name)
    if current_platform == "win32" then 
		linkoptions {"/INCLUDE:" .. name}
    else
        print("Force link error, current platform not support", current_platform)
    end 
end

function setup_project()
    setup_project_env()
    setup_platform()
    setup_project_definines()
end 

function setup_shaderinterop_includedir()
    includedirs { asset_location .. "/shaders/common" }
end 

-----------------------------------------------------------------------------------
-- main
-----------------------------------------------------------------------------------
function setup_env_from_options()
    if _OPTIONS["sln_name"] then 
        sln_name = _OPTIONS["sln_name"]
    end 
    if _OPTIONS["renderer"] then 
        renderer = _OPTIONS["renderer"]
    end 
    if _OPTIONS["platform_dir"] then
        platform_dir = _OPTIONS["platform_dir"]
    end 
    if _OPTIONS["env_dir"] then
        env_dir = _OPTIONS["env_dir"]
    end 
    if _OPTIONS["sdk_version"] then
        sdk_version = _OPTIONS["sdk_version"]
    end  
    if _OPTIONS["dynamic_plugins"] then	
        is_static_plugin = true
    end 
    if _OPTIONS["no_editor"] then	
        build_editor = false
    end 
    if _OPTIONS["build_app"] then	
        build_app = true
    end 
    if _OPTIONS["work_dir"] then
        work_dir = _OPTIONS["work_dir"]
    end 
    if _OPTIONS["project_dir"] then
        project_dir = _OPTIONS["project_dir"]
    end 
    if _OPTIONS["net_lib"] then 
        net_lib = _OPTIONS["net_lib"]
    end 
    if _OPTIONS["build_tests"] then 
        build_tests = true
    end 
    if _OPTIONS["header_tools_dir"] then 
        header_tools_dir = _OPTIONS["header_tools_dir"]
    end   
end

function setup_env_from_action()
    print("Target project version " .. _ACTION)

    if _ACTION == "vs2019" or _ACTION == "vs2022" then
        current_platform = "win32"
    end

    print("[premake]:sln_name:", sln_name)
    print("[premake]:env_dir:", env_dir)
    print("[premake]:work_dir:", work_dir)
    print("[premake]:project_dir", project_dir)
    print("[premake]:current_platform:", current_platform)
    print("[premake]:current renderer:", renderer and renderer or "NULL")

    if is_static_plugin then 
        print("[premake]:static plugins")
    else
        print("[premake]:dynamic plugins")
    end 

    print("[premake]:header tools dir:", header_tools_dir)
end 

setup_env_from_options()
setup_env_from_action()