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

-- LZ4
files 
{ 
    third_party_location .. "/lz4/**.h",
    third_party_location .. "/lz4/**.c",
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
