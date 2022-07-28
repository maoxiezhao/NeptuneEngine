#pragma once

#include "core\resource\resource.h"
#include "core\collections\hashMap.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\shader.h"

namespace VulkanTest
{
	struct RendererPlugin;

	class VULKAN_TEST_API Shader final : public Resource
	{
	public:
		DECLARE_RESOURCE(Shader);

#pragma pack(1)
		struct FileHeader
		{
			U32 magic;
			U32 version;
		};
#pragma pack()
		static const U32 FILE_MAGIC;
		static const U32 FILE_VERSION;

		struct ShaderContainer
		{
			HashMap<U64, GPU::Shader*> shaders;

			void Clear();
			void Add(GPU::Shader* shader, const String& name, I32 permutationIndex);
			GPU::Shader* Get(const String& name, I32 permutationIndex);

		private:
			U64 CalculateHash(const String& name, I32 permutationIndex);
		};

		Shader(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Shader();

		GPU::Shader* GetVS(const String& name, I32 permutationIndex = 0) {
			return GetShader(GPU::ShaderStage::VS, name, permutationIndex);
		}

		GPU::Shader* GetPS(const String& name, I32 permutationIndex = 0) {
			return GetShader(GPU::ShaderStage::PS, name, permutationIndex);
		}

		GPU::Shader* GetCS(const String& name, I32 permutationIndex = 0) {
			return GetShader(GPU::ShaderStage::CS, name, permutationIndex);
		}

		GPU::Shader* GetShader(GPU::ShaderStage stage, const String& name, I32 permutationIndex);

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		ShaderContainer shaders;
	};
}