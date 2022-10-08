#pragma once

#ifdef CJING3D_RENDERER_VULKAN

#include "shaderMeta.h"
#include "shaderReader.h"

namespace VulkanTest
{
	struct ShaderCompilationContext;

	class ShaderCompilerVulkan
	{
	public:
		ShaderCompilerVulkan(ShaderCompilationContext& context_);
		~ShaderCompilerVulkan();

		bool CompileShaders();

	private:
		bool CompileShader(ShaderFunctionMeta& meta, const char* includeDir);
		void WriteShaderInfo(ShaderFunctionMeta& meta);
		void WriteShaderFunctionPermutation(ShaderFunctionMeta& meta, I32 permutationIndex, const GPU::ShaderResourceLayout& resLayout, const void* cache, I32 cacheSize);

		template <typename T>
		void Write(const T& obj) {
			outMem.Write(&obj, sizeof(obj));
		}
		void Write(const void* ptr, size_t size) {
			outMem.Write(ptr, size);
		}

		ShaderCompilationContext& context;
		OutputMemoryStream& outMem;
		Mutex mutex;
	};
}

#endif
