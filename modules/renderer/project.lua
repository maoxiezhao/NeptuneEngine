-- [Engine moduler] Core
add_module_lib(PROJECT_RENDERER_NAME) 

-- Includedirs
includedirs { "" }

if renderer == "vulkan" then 
    -- vulkan header
    includedirs { third_party_location .. "/vulkan/include" }
end 

-- Files
files 
{
    "**.c",
    "**.cpp",
    "**.hpp",
    "**.h",
    "**.inl",
}

-- Shader interop
files 
{
    asset_location .. "/shaders/common/**.h"
}

vpaths { 
    ["shaders"] = {
        asset_location .. "/shaders/common/**.h"
    },
    ["*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}