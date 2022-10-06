#pragma once

#include "shaderMeta.h"
#include "shaderReader.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

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
        static bool Compile(EditorApp& editor, CompilationOptions& options);
        static bool Write(EditorApp& editor, Guid guid, const Path& inputPath, const Path& outputPath, OutputMemoryStream& mem);
	};
}
}
