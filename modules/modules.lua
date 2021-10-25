-- config
set_module_location( env_dir .. "modules")
set_build_location("../app/build/" ..  platform_dir .. "/modules")
set_lib_location("../app/bin/" ..  platform_dir .. "/libs")

-- definitions
PROJECT_CORE_NAME       = "core"
PROJECT_GPU_NAME        = "gpu"

-- default modules
register_module(PROJECT_CORE_NAME)
register_module(
    PROJECT_GPU_NAME,
    { 
        PROJECT_CORE_NAME 
    }
)

-- project premake files
create_modulen(PROJECT_CORE_NAME, "core/project.lua")
create_modulen(PROJECT_GPU_NAME, "gpu/project.lua")