-- [Engine moduler] Core
if registering then
    register_module(PROJECT_ECS_NAME)
end

if building then
    add_module_lib(PROJECT_ECS_NAME) 

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

    -- ignore
    removefiles { "ecs/test.cpp" }
end