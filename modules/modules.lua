----------------------------------------------------------------------------------------------------------
-- definitions
PROJECT_MATH_NAME       = "math"
PROJECT_CORE_NAME       = "core"
PROJECT_ECS_NAME        = "ecs"
PROJECT_GPU_NAME        = "gpu"
PROJECT_CONTENT_NAME    = "content"
PROJECT_RENDERER_NAME   = "renderer"
PROJECT_LEVEL_NAME      = "level"
PROJECT_CLIENT_NAME     = "client"
PROJECT_EDITOR_NAME     = "editor"
PROJECT_SHADERS_COMPILATION_NAME = "shaderCompilation"
PROJECT_CONTENT_IMPORTERS_NAME = "contentImporters"

-- config
local module_env = "../../"
set_module_location( module_env .. "modules")
set_build_location(module_env .. app_dir .. "/build/" ..  platform_dir .. "/modules")
set_lib_location(module_env .. app_dir .. "/bin/" ..  platform_dir .. "/libs")
set_third_party_location(module_env .. "3rdparty")
set_asset_location(module_env .. "assets")

print("---------------------------------------------------")
print("Create modules")
group "modules"

local modules = {}
local project_file_name = "**/project.lua"
local matches = os.matchfiles(project_file_name)

-- register modules
registering = true
building = false
for _, v in ipairs(matches) do 
    dofile(v)
end 

-- build modules
registering = false
building = true
for _, v in ipairs(matches) do 
    dofile(v)
end 

print("---------------------------------------------------")
group ""