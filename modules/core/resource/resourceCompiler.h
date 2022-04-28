#pragma once

#include "resource.h"

namespace VulkanTest
{
    class VULKAN_TEST_API ResourceCompiler
    {
    public:
        // Resource plugin used to support resource compiling
        struct VULKAN_TEST_API IPlugin
        {
            virtual ~IPlugin() {}
            virtual bool Compile(const char* path, FileSystem& fs) = 0;
        };
       
        static bool CompiledResource(const char* path, Span<const U8>data, FileSystem& fs);  
    };
}