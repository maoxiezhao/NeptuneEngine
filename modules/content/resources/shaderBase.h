#pragma once

#include "shaderStorage.h"
#include "content\binaryResource.h"
#include "core\collections\hashMap.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\shader.h"

namespace VulkanTest
{
	struct RendererPlugin;

	struct ShaderContainer
	{
		HashMap<U64, GPU::Shader*> shaders;

		bool LoadFromMemory(InputMemoryStream& inputMem);

		void Clear();
		void Add(GPU::Shader* shader, const String& name, I32 permutationIndex);
		GPU::Shader* Get(const String& name, I32 permutationIndex);

	private:
		U64 CalculateHash(const String& name, I32 permutationIndex);
	};

	class VULKAN_TEST_API ShaderResourceBase
	{
	public:
        struct ShaderCacheResult
        {
            OutputMemoryStream Data;

#if COMPILE_WITH_SHADER_COMPILER
            Array<String> Includes;
#endif
        };
		bool LoadShaderCache(ShaderCacheResult& result);

		static I32 GetCacheChunkIndex();

		virtual BinaryResource* GetShaderResource() const = 0;

#if CJING3D_EDITOR
		bool Save();
#endif

	protected:
		bool InitBase(ResourceInitData& initData);

		ShaderStorage::Header shaderHeader;
	};

	template<typename BaseType>
	class ShaderResourceTypeBase : public BaseType, public ShaderResourceBase
	{
	public:
		explicit ShaderResourceTypeBase(const ResourceInfo& info)
			: BaseType(info)
		{
		}

		AssetChunksFlag GetChunksToPreload()const override 
		{
			AssetChunksFlag flags = 0;
			const auto cachingMode = ShaderStorage::CurrentCachingMode;
			if (cachingMode == ShaderStorage::CachingMode::ResourceInternal)
				flags |= GET_CHUNK_FLAG(GetCacheChunkIndex());
			return flags;
		}

		BinaryResource* GetShaderResource() const override
		{
			return (BinaryResource*)this;
		}

	protected:
		bool Init(ResourceInitData& initData)override
		{
			return InitBase(initData);
		}
	};
}