#include "materialParams.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	bool MaterialParams::Load(U64 size, const U8* mem, ResourceManager& resManager)
	{
		if (size <= 0)
			return false;

		InputMemoryStream stream(mem, size);
		I32 paramsCount;
		stream.Read(paramsCount);
		params.resize(paramsCount);

		for (I32 i = 0; i < paramsCount; i++)
		{
			auto& param = params[i];
			param.type = static_cast<MaterialParameterType>(stream.Read<U8>());
			param.name = stream.ReadStringWithLength();
			param.registerIndex = stream.Read<U8>();
			param.offset = stream.Read<U16>();

			switch (param.type)
			{
			case MaterialParameterType::Bool:
				param.asBool = stream.Read<bool>();
				break;
			case MaterialParameterType::Integer:
				stream.Read(param.asInteger);
				break;
			case MaterialParameterType::Float:
				stream.Read(param.asFloat);
				break;
			case MaterialParameterType::Vector2:
				stream.Read(param.asVector2);
				break;
			case MaterialParameterType::Vector3:
				stream.Read(param.asVector3);
				break;
			case MaterialParameterType::Color:
				param.asColor = stream.Read<U32>();
				break;
			case MaterialParameterType::Texture:
			{
				String path = stream.ReadStringWithLength();
				param.asTexture = resManager.LoadResourcePtr<Texture>(Path(path.c_str()));
			}
				break;
			default:
				break;
			}
		}
		return true;
	}

	void MaterialParams::Save(OutputMemoryStream& mem)
	{
		mem.Write((I32)params.size());

		for (U32 i = 0; i < params.size(); i++)
		{
			auto& param = params[i];
			mem.Write((U8)param.type);
			mem.WriteStringWithLength(param.name);
			mem.Write(param.registerIndex);
			mem.Write(param.offset);

			switch (param.type)
			{
			case MaterialParameterType::Bool:
				mem.Write(param.asBool);
				break;
			case MaterialParameterType::Integer:
				mem.Write(param.asInteger);
				break;
			case MaterialParameterType::Float:
				mem.Write(param.asFloat);
				break;
			case MaterialParameterType::Vector2:
				mem.Write(param.asVector2);
				break;
			case MaterialParameterType::Vector3:
				mem.Write(param.asVector3);
				break;
			case MaterialParameterType::Color:
				mem.Write(param.asColor);
				break;
			case MaterialParameterType::Texture:
			{
				ASSERT(param.asTexture);
				String path(param.asTexture->GetPath().c_str());
				mem.WriteStringWithLength(path);
			}
			break;
			default:
				break;
			}
		}
	}

	void MaterialParams::Unload()
	{
		params.clear();
	}

	void MaterialParams::Save(OutputMemoryStream& mem, const Array<SerializedMaterialParam>& params)
	{
		mem.Write((I32)params.size());

		for (U32 i = 0; i < params.size(); i++)
		{
			auto& param = params[i];
			mem.Write((U8)param.type);
			mem.WriteStringWithLength(param.name);
			mem.Write(param.registerIndex);
			mem.Write(param.offset);

			switch (param.type)
			{
			case MaterialParameterType::Bool:
				mem.Write(param.asBool);
				break;
			case MaterialParameterType::Integer:
				mem.Write(param.asInteger);
				break;
			case MaterialParameterType::Float:
				mem.Write(param.asFloat);
				break;
			case MaterialParameterType::Vector2:
				mem.Write(param.asVector2);
				break;
			case MaterialParameterType::Vector3:
				mem.Write(param.asVector3);
				break;
			case MaterialParameterType::Color:
				mem.Write(param.asColor);
				break;
			case MaterialParameterType::Texture:
			{
				String path(param.asPath.c_str());
				mem.WriteStringWithLength(path);
			}
			break;
			default:
				break;
			}
		}
	}

	MaterialParam* MaterialParams::Get(const String& name)
	{
		MaterialParam* ret = nullptr;
		for (auto& param : params)
		{
			if (param.name == name)
			{
				ret = &param;
				break;
			}
		}
		return ret;
	}

	MaterialParam* MaterialParams::GetTexture(Texture::TextureType type)
	{
		MaterialParam* ret = nullptr;
		for (auto& param : params)
		{
			if (param.type == MaterialParameterType::Texture && param.registerIndex == (U8)type)
			{
				ret = &param;
				break;
			}
		}
		return ret;
	}

	I32 MaterialParams::Find(const String& name)
	{
		I32 ret = -1;
		for (I32 i = 0; i < (I32)params.size(); i++)
		{
			if (params[i].name == name)
			{
				ret = i;
				break;
			}
		}
		return ret;
	}

	void DefaultMaterialParams::Load(MaterialParams& params)
	{
		baseColor = params.Get("BaseColor");
		baseColorMap = params.GetTexture(Texture::TextureType::DIFFUSE);
	}

	void DefaultMaterialParams::Unload()
	{
		baseColor = nullptr;
		baseColorMap = nullptr;
	}

	void DefaultMaterialParams::WriteShaderMaterial(ShaderMaterial* dest)
	{
		ShaderMaterial shaderMaterial = {};
		shaderMaterial.baseColor = baseColor ? Color4(baseColor->asColor).ToFloat4() : F32x4(1.0f);

		auto device = Renderer::GetDevice();
		auto GetTextureIndex = [device](Texture* texture)->int {
			if (texture == nullptr || !texture->IsReady())
				return -1;

			return texture->GetDescriptorIndex();
		};

		shaderMaterial.texture_basecolormap_index = baseColorMap ? GetTextureIndex(baseColorMap->asTexture.get()) : -1;

		memcpy(dest, &shaderMaterial, sizeof(ShaderMaterial));
	}
}