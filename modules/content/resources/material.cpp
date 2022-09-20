#include "material.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Material);

	Material::Material(const ResourceInfo& info) :
		BinaryResource(info)
	{
	}

	Material::~Material()
	{
	}

	void Material::WriteShaderMaterial(ShaderMaterial* dest)
	{
		ASSERT(IsReady());
		defaultParams.WriteShaderMaterial(dest);
	}

	void Material::Bind(MaterialShader::BindParameters& params)
	{
		ASSERT(IsReady());
		materialShader->Bind(params);
	}

	bool Material::IsReady() const
	{
		return (Resource::IsReady() && materialShader && materialShader->IsReady());
	}

	const Shader* Material::GetShader() const
	{
		return materialShader ? materialShader->GetShader() : nullptr;
	}

	bool Material::Init(ResourceInitData& initData)
	{
		if (initData.customData.Size() < sizeof(MaterialHeader))
		{
			Logger::Warning("Unsupported material file %s", GetPath());
			return false;
		}

		memcpy(&header, initData.customData.Data(), sizeof(MaterialHeader));

		if (header.magic != MaterialHeader::MAGIC)
		{
			Logger::Warning("Unsupported material file %s", GetPath());
			return false;
		}

		if (header.version != MaterialHeader::LAST_VERSION)
		{
			Logger::Warning("Unsupported version of material %s", GetPath());
			return false;
		}

		return true;
	}

	bool Material::Load()
	{
		PROFILE_FUNCTION();
		ASSERT(materialShader == nullptr);

		// Create material shader
		InputMemoryStream shaderStream(nullptr, 0);
		const auto shaderChunk = GetChunk(MATERIAL_CHUNK_SHADER_SOURCE);
		if (shaderChunk && shaderChunk->IsLoaded())
			shaderStream = InputMemoryStream(shaderChunk->Data(), shaderChunk->Size());

		materialShader = MaterialShader::Create(String(GetPath().c_str()), shaderStream, header.materialInfo);
		if (materialShader == nullptr)
		{
			Logger::Warning("Failed to create material shader %s", GetPath().c_str());
			return false;
		}

		// Load material params
		const auto paramsChunk = GetChunk(MATERIAL_CHUNK_PARAMS);
		if (paramsChunk != nullptr && paramsChunk->IsLoaded())
		{
			if (!params.Load(paramsChunk->Size(), paramsChunk->Data()))
			{
				Logger::Warning("Failed to load material params %s", GetPath().c_str());
				return false;
			}

			// Load default params
			defaultParams.Load(params);
		}

		return true;
	}

	void Material::Unload()
	{
		if (materialShader != nullptr)
		{
			materialShader->Unload();
			CJING_SAFE_DELETE(materialShader);
			materialShader = nullptr;
		}
		defaultParams.Unload();
		params.Unload();
	}

	AssetChunksFlag Material::GetChunksToPreload() const
	{
		return GET_CHUNK_FLAG(MATERIAL_CHUNK_SHADER_SOURCE) | GET_CHUNK_FLAG(MATERIAL_CHUNK_PARAMS);
	}
}