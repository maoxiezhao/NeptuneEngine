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

-- fretype
includedirs 
{ 
    third_party_location.. "/freetype/include"
}
libdirs { third_party_location.. "/freetype/lib" }
links {"freetype"}

-- Shader interop
files 
{
    asset_location .. "/shaders/common/**.h"
}
includedirs { asset_location .. "/shaders/common" }

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