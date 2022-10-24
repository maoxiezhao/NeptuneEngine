-- [Engine moduler] Core
if registering then
    register_module(PROJECT_LEVEL_NAME,    { PROJECT_RENDERER_NAME })
end

if building then
    add_module_lib(PROJECT_LEVEL_NAME) 

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

    if renderer == "vulkan" then 
        -- vulkan header
        includedirs { third_party_location .. "/vulkan/include" }
    end
end