-- [Engine moduler] Core
if registering then
    register_module(PROJECT_CLIENT_NAME,   { PROJECT_RENDERER_NAME })
end

if building then
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

    setup_shaderinterop_includedir()
end