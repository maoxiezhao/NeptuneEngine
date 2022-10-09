#pragma once

#include "core\types\guid.h"
#include "shaderCompilationContext.h"
#include "shaderMeta.h"
#include "shaderReader.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ShaderCompilation
	{
	public:
        static bool Compile(CompilationOptions& options);
        static bool Write(Guid guid, const Path& inputPath, const Path& outputPath, OutputMemoryStream& mem);
	};
}
