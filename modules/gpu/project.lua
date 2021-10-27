-- [Engine moduler] gpu
add_module_lib(PROJECT_GPU_NAME)
   
 -- Includedirs
 includedirs { "" }
 includedirs { third_party_location .. "/vulkan/include" }
 files 
 {
     "**.c",
     "**.cpp",
     "**.hpp",
     "**.h",
     "**.inl",

     third_party_location .. "/volk/volk.h",
     third_party_location .. "/volk/volk.c",

     third_party_location .. "/spriv_reflect/spirv_reflect.h",
     third_party_location .. "/spriv_reflect/spirv_reflect.c",
 }

 vpaths { 
     ["3rdparty/volk"] = {
         third_party_location .. "/volk/volk.h",
         third_party_location .. "/volk/volk.c",
     },
     ["3rdparty/spriv_reflect"] = {
         third_party_location .. "/spriv_reflect/spirv_reflect.h",
         third_party_location .. "/spriv_reflect/spirv_reflect.c",
     },
     ["gpu/*"] = {
         "**.c",
         "**.cpp",
         "**.hpp",
         "**.h",
         "**.inl",
     }
 }