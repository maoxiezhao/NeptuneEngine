#pragma once

#include "shaderMeta.h"
#include "shaderReader.h"
#include "shaderCompilationContext.h"

namespace VulkanTest
{
	struct ShaderCompilationContext;

	enum class ShaderProfile
	{
		Unknown = 0,
		DirectX = 1, //SM 6
		Vulkan = 2,
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler(ShaderProfile profile_);
		~ShaderCompiler();

		bool CompileShaders(ShaderCompilationContext* context_);

		static bool GetIncludedFileSource(ShaderCompilationContext* context, const char* sourceFile, const char* includedFile, const char*& source, I32& sourceLength);
		static void FreeIncludeFileCache();

		FORCE_INLINE ShaderProfile GetProfile() const {
			return profile;
		}

	protected:
		template <typename T>
		void Write(const T& obj) {
			outMem->Write(&obj, sizeof(obj));
		}
		void Write(const void* ptr, size_t size) {
			outMem->Write(ptr, size);
		}

	protected:
		virtual bool CompileShader(ShaderFunctionMeta& meta) = 0;

		bool WriteShaderInfo(ShaderFunctionMeta& meta);
		bool WriteShaderFunctionPermutation(ShaderFunctionMeta& meta, I32 permutationIndex, const GPU::ShaderResourceLayout& resLayout, const void* cache, I32 cacheSize);

	protected:
		ShaderProfile profile;
		ShaderCompilationContext* context = nullptr;
		OutputMemoryStream* outMem = nullptr;
	};
}