#pragma once

#include "core\resource\binaryResource.h"
#include "core\resource\resourceManager.h"
#include "core\scripts\luaConfig.h"
#include "materials\materialShader.h"

namespace VulkanTest
{
	struct MaterialFactory;

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

	struct MaterialParams
	{
		enum FLAGS
		{
			EMPTY = 0,
			DIRTY = 1 << 0,
			CAST_SHADOW = 1 << 1,
			DOUBLE_SIDED = 1 << 2,
		};

		Color4 color;
		U32 flags = EMPTY;
		ResPtr<Texture> textures[Texture::TextureType::COUNT];

		bool Load(U64 size, const U8* mem, MaterialFactory& factory);
		void Unload();
	};

	class VULKAN_TEST_API Material final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(Material);

		Material(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Material();

		void Bind(MaterialShader::BindParameters& params);

		MaterialParams& GetParams(){
			return params;
		}

		const MaterialParams& GetParams()const {
			return params;
		}

		bool IsReady()const override;
		const Shader* GetShader()const;

	protected:
		bool OnLoaded()override;
		void OnUnLoaded() override;
		AssetChunksFlag GetChunksToPreload()const override;

	private:
		MaterialParams params;
		MaterialShader* materialShader = nullptr;
	};

	struct MaterialFactory : public BinaryResourceFactory
	{
	public:
		MaterialFactory();
		virtual ~MaterialFactory();

		LuaConfig& GetLuaConfig() {
			return luaConfig;
		}

	protected:
		Resource* CreateResource(const Path& path) override;
		void DestroyResource(Resource* res) override;

	private:
		LuaConfig luaConfig;
	};
}