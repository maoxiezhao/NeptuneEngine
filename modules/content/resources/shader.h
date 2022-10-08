#pragma once

#include "shaderBase.h"
#include "content\binaryResource.h"
#include "core\collections\hashMap.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\shader.h"

namespace VulkanTest
{
	struct RendererPlugin;

	class VULKAN_TEST_API Shader final : public ShaderResourceTypeBase<BinaryResource>
	{
	public:
		DECLARE_RESOURCE(Shader);

		Shader(const ResourceInfo& info);
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
		bool Load()override;
		void Unload() override;

	private:
		ShaderStorage::Header header;
		ShaderContainer shaders;
	};
}