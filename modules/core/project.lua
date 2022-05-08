-- [Engine moduler] Core
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
    ["*"] = {
        "**.c",
        "**.cpp",
        "**.hpp",
        "**.h",
        "**.inl",
    }
}
