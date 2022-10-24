-- [Engine moduler] Core
if registering then
    register_module(PROJECT_MATH_NAME)
end

if building then
    add_module_lib(PROJECT_MATH_NAME) 

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
end