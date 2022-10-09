#pragma once

#include "shaderMeta.h"

namespace VulkanTest
{
    /// <summary>
    /// Shader compilation options container
    /// </summary>
    struct CompilationOptions
    {
        String targetName;
        Guid targetGuid = Guid::Empty;
        const char* source = nullptr;
        U32 sourceLength = 0;
        Array<GPU::ShaderMacro> Macros;
        OutputMemoryStream* outMem;
        Array<Path> includes;
    };

    /// <summary>
    /// Shader compilation context container
    /// </summary>
    struct ShaderCompilationContext
    {
        CompilationOptions* options;
        ShaderMeta* shaderMeta;
        OutputMemoryStream* source;
        Array<Path> includes;
    };
}