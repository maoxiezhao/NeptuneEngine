
all_engine_modules = {}
default_engine_modules = {}
dependencies_mapping = {}
module_location = ""
build_location = ""
lib_location = ""

function set_module_location(location)
    module_location = location
end 

function set_build_location(location)
    build_location = location
end 

function set_lib_location(location)
    lib_location = location
end 

function register_module(project_name, dependencies, is_default)
    if is_default == nil then 
        is_default = true
    end

    table.insert(all_engine_modules, project_name)

    if is_default then 
        table.insert(default_engine_modules, project_name)
    end 

    if dependencies then 
        dependencies_mapping[project_name] = dependencies
    end 
end 

function get_all_dependencies(project_name, hash_map)
    if hash_map[project_name] and hash_map[project_name] == 1 then 
        return
    end 
    hash_map[project_name] = 1

    local dependencies = dependencies_mapping[project_name]
    if dependencies == nil or #dependencies <= 0 then
        return
    end 
    
    for _, dependency in ipairs(dependencies) do
        get_all_dependencies(dependency, hash_map)
    end 
end 

function setup_module_dependencies(project_name)
    local dependencies = dependencies_mapping[project_name]
    if dependencies ~= nil then 
        for _, dependency in ipairs(dependencies) do
            dependson { dependency }
        end 
    end 
end 

function setup_module_dependent_libs(project_name, config)
    local dependencies = dependencies_mapping[project_name]
    if dependencies ~= nil then 
        local dependent_map = {}
        for _, dependency in ipairs(dependencies) do 
            get_all_dependencies(dependency, dependent_map)
        end 
    
        for dependency, v in pairs(dependent_map) do 
            libdirs {lib_location .. "/" .. config}
            links {dependency}
            -- includedirs {"../" .. dependency .. "/src"}
        end 
    end 
end 

function setup_modules(config, engine_modules)
    -- include
    for _, project_name in ipairs(engine_modules) do 
        includedirs {env_dir .. "modules"}
    end 

    -- lib links
    for _, project_name in ipairs(engine_modules) do 
        links {project_name}
    end
    
    -- dependencies
    for _, project_name in ipairs(engine_modules) do 
        dependson { project_name }
    end 
end 

function create_modulen(module_name, module_file)
    print("[Module]", module_name)
    project (module_name)
    location(build_location.. "/" .. module_name)
    objdir(build_location .. "/" .. module_name .. "/temp")
    kind "StaticLib"
    language "C++"
    conformanceMode(true)
    setup_project()
    targetname(module_name)
    setup_module_dependencies(module_name)

    -- includes
    local THIRD_PARTY = "../3rdparty"
    includedirs {
        module_location,
        module_location .. "/" .. module_name,
        THIRD_PARTY, 
    }

    -- Debug config
    filter {"configurations:Debug"}
        targetdir (lib_location .. "/Debug")
        setup_module_dependent_libs(module_name, "Debug")
        defines { "DEBUG" }

    -- Release config
    filter {"configurations:Release"}
        targetdir (lib_location .. "/Release")
        defines { "NDEBUG" }
        setup_module_dependent_libs(module_name, "Release")

    filter { }

    -- do module lua file
    if module_file ~= nil and module_file ~= "" then 
        dofile(module_file)
    end 
end