dofile("../../tools/cjing_build/premake/options.lua")
dofile("../../tools/cjing_build/premake/globals.lua")
dofile("options.lua")

local build_shader_libs = false;
if _OPTIONS["build_shader_libs"] then	
    build_shader_libs = true
end 
local build_examples = false;
if _OPTIONS["build_examples"] then
    build_examples = true
end 
local build_tests = false;
if _OPTIONS["build_tests"] then 
    build_tests = true
end 

solution "glfw_sln"
	location ("build/" .. platform_dir ) 
	configurations { "Debug", "Release" }
	setup_project_env()
	
dofile("src/project.lua")