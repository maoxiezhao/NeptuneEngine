local function create_test_instance(name, source_files)
    create_example_test(
        name,                           -- project_name
        "src",                          -- source_directory
        source_files,                   -- source_files
        "../app",                       -- target_directory
        "ConsoleApp",                   -- app kind
        nil,                            -- plugins,
        default_engine_modules,         -- engine modules
        function(SOURCE_DIR)
            -- vulkan header
            includedirs { "../3rdparty/vulkan/include" }
        end
    )
end 

group "tests"
create_test_instance("bindlessTest",  { "bindlessTest.cpp"} )
create_test_instance("triangleTest",  { "triangleTest.cpp"} )
create_test_instance("particleDream", { "particleDream.cpp"} )
create_test_instance("jobsystemTest", { "jobsystemTest.cpp"} )
create_test_instance("renderGraphTest", { "renderGraphTest.cpp"} )
group ""