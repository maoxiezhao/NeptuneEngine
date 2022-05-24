-- [Engine moduler] Core
add_module_lib(PROJECT_CLIENT_NAME) 

-- Includedirs
includedirs { "" }
includedirs { third_party_location .. "/vulkan/include" }

-- Files
files 
{
    "**.c",
    "**.cpp",
    "**.hpp",
    "**.h",
    "**.inl",
}