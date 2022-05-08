-- config
local module_env = "../../"
set_module_location( module_env .. "modules")
set_build_location(module_env .. "app/build/" ..  platform_dir .. "/modules")
set_lib_location(module_env .. "app/bin/" ..  platform_dir .. "/libs")
set_third_party_location(module_env .. "3rdparty")
set_asset_location(module_env .. "assets")

-- definitions
PROJECT_MATH_NAME       = "math"
PROJECT_CORE_NAME       = "core"
PROJECT_ECS_NAME        = "ecs"
PROJECT_GPU_NAME        = "gpu"
PROJECT_RENDERER_NAME   = "renderer"
PROJECT_CLIENT_NAME     = "client"
PROJECT_EDITOR_NAME     = "editor"

register_module(PROJECT_MATH_NAME)
register_module(PROJECT_ECS_NAME)
register_module(PROJECT_CORE_NAME,     { PROJECT_MATH_NAME, PROJECT_ECS_NAME })
register_module(PROJECT_GPU_NAME,      { PROJECT_CORE_NAME })
register_module(PROJECT_RENDERER_NAME, { PROJECT_GPU_NAME })
register_module(PROJECT_CLIENT_NAME,   { PROJECT_RENDERER_NAME })
register_module(PROJECT_EDITOR_NAME,   { PROJECT_CLIENT_NAME })

print("---------------------------------------------------")
print("Create modules")
group "modules"
local project_file_name = "**/project.lua"
local matches = os.matchfiles(project_file_name)
for _, v in ipairs(matches) do 
    dofile(v)
end 
print("---------------------------------------------------")
group ""