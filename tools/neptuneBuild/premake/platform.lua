
local function setup_platform_win32()
    systemversion(windows_sdk_version())
end 

function setup_platform()
    if platform_dir == "win32" then 
        setup_platform_win32()
    end 
end 