
all_engine_modules = {}
default_engine_modules = {}
dependencies_mapping = {}

function register_engine_module(project_name, dependencies, is_default)
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

function setup_dependencies(project_name)
    local dependencies = dependencies_mapping[project_name]
    if dependencies ~= nil then 
        for _, dependency in ipairs(dependencies) do
            dependson { dependency }
        end 
    end 
end 

function setup_dependent_libs(project_name, config)
    local dependencies = dependencies_mapping[project_name]
    if dependencies ~= nil then 
        local dependency_map = {}
        for _, dependency in ipairs(dependencies) do 
            get_all_dependencies(dependency, dependency_map)
        end 
    
        for dependency, v in pairs(dependency_map) do 
            libdirs {"../" .. dependency .. "/lib/" .. config}
            links {dependency}
            includedirs {"../" .. dependency .. "/src"}
        end 
    end 
end 

function setup_engine(config, dependencies)
    -- include
    for _, project_name in ipairs(dependencies) do 
        includedirs {env_dir .. "src/" .. project_name .. "/src"}
    end 

    -- libs
    -- lib dirs
    for _, project_name in ipairs(dependencies) do 
        libdirs {env_dir .. "src/" .. project_name .. "/lib/" .. config}
    end 

    -- lib links
    for _, project_name in ipairs(dependencies) do 
        links {project_name}
    end
    

    -- dependencies
    for _, project_name in ipairs(dependencies) do 
        dependson { project_name }
    end 
end 