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

    third_party_location .. "/imnodes/imnodes_internal.h",
    third_party_location .. "/imnodes/imnodes.h",
    third_party_location .. "/imnodes/imnodes.cpp",
}

-- removefiles 
-- { 
--     third_party_location .. "/imgui-docking/imgui_demo.cpp",
-- }

vpaths { 
    ["imgui"] = {
        third_party_location .. "/imgui-docking/**.h",
        third_party_location .. "/imgui-docking/**.cpp",

        third_party_location .. "/imnodes/imnodes_internal.h",
        third_party_location .. "/imnodes/imnodes.h",
        third_party_location .. "/imnodes/imnodes.cpp",
    },
    ["*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}

if renderer == "vulkan" then 
    -- vulkan header
    includedirs { third_party_location .. "/vulkan/include" }
end 

setup_shaderinterop_includedir()