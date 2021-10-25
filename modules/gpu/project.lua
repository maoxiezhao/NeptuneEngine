-- [Engine moduler] gpu
 
local THIRD_PARTY = "../../3rdparty"

-- Files
files 
{ 
    "**.c",
    "**.cpp",
    "**.hpp",
    "**.h",
    "**.inl",
}

-- Vulkan
includedirs { THIRD_PARTY .. "/vulkan/include" }
files 
{
    THIRD_PARTY .. "/volk/volk.h",
    THIRD_PARTY .. "/volk/volk.c",

    THIRD_PARTY .. "/spriv_reflect/spirv_reflect.h",
    THIRD_PARTY .. "/spriv_reflect/spirv_reflect.c",
}

vpaths { 
    ["3rdparty/volk"] = {
        THIRD_PARTY .. "/volk/volk.h",
        THIRD_PARTY .. "/volk/volk.c",
    },
    ["3rdparty/spriv_reflect"] = {
        THIRD_PARTY .. "/spriv_reflect/spirv_reflect.h",
        THIRD_PARTY .. "/spriv_reflect/spirv_reflect.c",
    },
    ["gpu/*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}