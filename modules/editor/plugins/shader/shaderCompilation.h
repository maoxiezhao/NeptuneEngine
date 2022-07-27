#pragma once

#include "shaderMeta.h"
#include "shaderReader.h"
#include "renderer\shader.h"

namespace VulkanTest
{
namespace Editor
{
    struct CompilationOptions
    {
        Path path;
        Array<GPU::ShaderMacro> Macros;
        OutputMemoryStream* outMem;
        Array<Path> dependencies;
    };

    struct ShaderCompilationContext
    {
        CompilationOptions* options;
        ShaderMeta* shaderMeta;
        OutputMemoryStream* source;
    };

	class VULKAN_EDITOR_API ShaderCompilation
	{
	public:
        static bool Compile(class EditorApp& editor, CompilationOptions& options);
	};
}
}
