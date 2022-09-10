-- [Engine moduler] Core
add_module_lib(PROJECT_CONTENT_NAME) 

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

-- LZ4
files 
{ 
    third_party_location .. "/lz4/**.h",
    third_party_location .. "/lz4/**.c",
}

-- Shader interop
files 
{
    asset_location .. "/shaders/common/**.h"
}
includedirs { asset_location .. "/shaders/common" }

vpaths { 
    ["compress/lz4"] = {
        third_party_location .. "/lz4/**.h",
        third_party_location .. "/lz4/**.c",
    },
    ["*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}
