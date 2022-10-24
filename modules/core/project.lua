-- [Engine moduler] Core
if registering then
    register_module(PROJECT_CORE_NAME,     { PROJECT_MATH_NAME, PROJECT_ECS_NAME })
end

if building then
    add_module_lib(PROJECT_CORE_NAME) 

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

    -- Lua
    files 
    { 
        third_party_location .. "/lua/**.h",
        third_party_location .. "/lua/**.c",
    }

    vpaths { 
        ["scripts/lua"] = {
            third_party_location .. "/lua/**.h",
            third_party_location .. "/lua/**.c",
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