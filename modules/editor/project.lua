-- [Engine moduler] Core
add_module_lib(PROJECT_EDITOR_NAME) 

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

-- Imgui
files 
{ 
    third_party_location .. "/imgui-docking/**.h",
    third_party_location .. "/imgui-docking/**.cpp",
}