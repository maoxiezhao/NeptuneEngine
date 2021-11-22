-- [Engine moduler] Core
add_module_lib(PROJECT_CLIENT_NAME) 

-- Includedirs
includedirs { "" }

-- Files
files 
{
    "**.c",
    "**.cpp",
    "**.hpp",
    "**.h",
    "**.inl",
}

-- glfw
includedirs { third_party_location .. "/vulkan/include" }
includedirs { third_party_location .. "/glfw/include" }
libdirs {  third_party_location .. "/glfw/lib/" .. platform_dir,  }
filter {"configurations:Debug"}
    links {"glfw_lib_d"}
filter {"configurations:Release"}
    links {"glfw_lib"}
filter {}