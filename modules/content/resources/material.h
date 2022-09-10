#pragma once

#include "content\binaryResource.h"
#include "core\scripts\luaConfig.h"
#include "materialShader.h"
#include "materialParams.h"

namespace VulkanTest
{
#define MATERIAL_CHUNK_SHADER_SOURCE 0
#define MATERIAL_CHUNK_PARAMS 1

#pragma pack(1)
	struct MaterialHeader
	{
		static constexpr U32 LAST_VERSION = 0;
		static constexpr U32 MAGIC = '_CST';
		U32 magic = MAGIC;
		U32 version = LAST_VERSION;
		MaterialInfo materialInfo;
	};
#pragma pack()

	class VULKAN_TEST_API Material final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(Material);

		Material(const ResourceInfo& info, ResourceManager& resManager);
		virtual ~Material();

		void WriteShaderMaterial(ShaderMaterial* dest);
		void Bind(MaterialShader::BindParameters& params);

		MaterialParam* GetParam(const String& name) {
			return params.Get(name);
		}

		MaterialParams& GetParams(){
			return params;
		}

		const MaterialParams& GetParams()const {
			return params;
		}

		bool IsReady()const override;
		const Shader* GetShader()const;

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;
		AssetChunksFlag GetChunksToPreload()const override;

	private:
		MaterialHeader header;
		MaterialParams params;
		MaterialShader* materialShader = nullptr;
		DefaultMaterialParams defaultParams;
	};
}