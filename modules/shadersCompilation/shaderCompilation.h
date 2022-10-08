#pragma once

#include "core\types\guid.h"
#include "shaderMeta.h"
#include "shaderReader.h"

namespace VulkanTest
{
    struct CompilationOptions
    {
        String targetName;
        Guid targetGuid = Guid::Empty;
        const char* source = nullptr;
        U32 sourceLength = 0;
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

	class VULKAN_TEST_API ShaderCompilation
	{
	public:
        static bool Compile(CompilationOptions& options);
        static bool Write(Guid guid, const Path& inputPath, const Path& outputPath, OutputMemoryStream& mem);
	};
}
