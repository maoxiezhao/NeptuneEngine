-- [Engine moduler] Core
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