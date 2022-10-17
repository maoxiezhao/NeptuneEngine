-- [Engine moduler] Core
add_module_lib(PROJECT_CONTENT_IMPORTERS_NAME) 

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
includedirs { asset_location .. "/shaders/common" }

vpaths { 
    ["shaderInterop"] = {
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