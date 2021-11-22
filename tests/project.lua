group "tests"
create_example_app(
    "test",                         -- project_name
    "src",                          -- source_directory
    get_current_script_path(),      -- root_directory
    "ConsoleApp",                   -- app kind
    nil,                            -- plugins,
    default_engine_modules,         -- engine modules
    function(SOURCE_DIR)
        -- vulkan header
        includedirs { "../3rdparty/vulkan/include" }
    end
)
group ""