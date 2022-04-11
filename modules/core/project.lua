-- [Engine moduler] Core
add_module_lib(PROJECT_CORE_NAME) 

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

removefiles 
{ 
    third_party_location .. "/imgui-docking/imgui_demo.cpp",
}

vpaths { 
    ["imgui"] = {
        third_party_location .. "/imgui-docking/**.h",
        third_party_location .. "/imgui-docking/**.cpp",
    },
    ["*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}