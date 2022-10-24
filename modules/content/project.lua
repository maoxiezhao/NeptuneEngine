-- [Engine moduler] Core
if registering then
    if with_shader_compiler then 
        register_module(PROJECT_CONTENT_NAME,  { PROJECT_GPU_NAME, PROJECT_SHADERS_COMPILATION_NAME })
    else
        register_module(PROJECT_CONTENT_NAME,  { PROJECT_GPU_NAME })
    end
end

if building then
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
        ["shaders"] = {
            asset_location .. "/shaders/common/**.h"
        },
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
end