-- config
local module_env = "../../"
set_module_location( module_env .. "modules")
set_build_location(module_env .. "app/build/" ..  platform_dir .. "/modules")
set_lib_location(module_env .. "app/bin/" ..  platform_dir .. "/libs")
set_third_party_location(module_env .. "3rdparty")

-- definitions
PROJECT_CORE_NAME       = "core"
PROJECT_GPU_NAME        = "gpu"

register_module(PROJECT_CORE_NAME)
register_module(PROJECT_GPU_NAME, 
    { PROJECT_CORE_NAME }
)

print("---------------------------------------------------")
print("Create modules")
local project_file_name = "**/project.lua"
local matches = os.matchfiles(project_file_name)
for _, v in ipairs(matches) do 
    dofile(v)
end 
print("---------------------------------------------------")