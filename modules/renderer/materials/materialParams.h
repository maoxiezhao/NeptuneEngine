#pragma once

#include "core\resource\binaryResource.h"
#include "core\resource\resourceManager.h"
#include "renderer\texture.h"
#include "shaderInterop_renderer.h"

namespace VulkanTest
{
    enum class MaterialParameterType : U8
    {
        Invalid = 0,
        Bool,
        Integer,
        Float,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Texture,
    };

	struct SerializedMaterialParam
	{
		MaterialParameterType type;
		String name;
		U8 registerIndex;
		U16 offset;
		union
		{
			bool asBool;
			I32 asInteger;
			F32 asFloat;
			F32x2 asVector2;
			F32x3 asVector3;
			U32 asColor;
		};
		Path asPath;
	};

	struct MaterialParam
	{
		MaterialParameterType type = MaterialParameterType::Invalid;
		String name;
		U8 registerIndex;
		U16 offset;
		union
		{
			bool asBool;
			I32 asInteger;
			F32 asFloat;
			F32x2 asVector2;
			F32x3 asVector3;
			U32 asColor;
		};
		ResPtr<Texture> asTexture;
	};

	struct MaterialParams
	{
	public:
		MaterialParam* Get(const String& name);
		MaterialParam* GetTexture(Texture::TextureType type);
		I32 Find(const String& name);

		const Array<MaterialParam>& GetParams()const {
			return params;
		}
		Array<MaterialParam>& GetParams() {
			return params;
		}

		bool Load(U64 size, const U8* mem, ResourceManager& resManager);
		void Save(OutputMemoryStream& mem);
		void Unload();
	
		static void Save(OutputMemoryStream& mem, const Array<SerializedMaterialParam>& params);

	private:
		Array<MaterialParam> params;
	};

	struct DefaultMaterialParams
	{
		MaterialParam* baseColor;
		MaterialParam* roughness;
		MaterialParam* metalness;

		MaterialParam* baseColorMap;
		MaterialParam* normalMap;

		void Load(MaterialParams& params);
		void Unload();
		void WriteShaderMaterial(ShaderMaterial* dest);
	};
}