#include "materialGenerator.h"

namespace VulkanTest
{
namespace Editor
{
	MaterialGenerator::MaterialGenerator()
	{
	}

	MaterialGenerator::~MaterialGenerator()
	{
	}

	SerializedMaterialParam& MaterialGenerator::FindOrCreateEmpty(const String& name, MaterialParameterType type)
	{
		SerializedMaterialParam& param = params.emplace();
		param.type = type;
		param.name = name;
		return param;
	}

	SerializedMaterialParam& MaterialGenerator::FindOrAddTexture(Texture::TextureType type)
	{
		for (auto& param : params)
		{
			if (param.type == MaterialParameterType::Texture && param.registerIndex == (U8)type)
				return param;
		}

		SerializedMaterialParam& param = params.emplace();
		param.type = MaterialParameterType::Texture;
		param.name = "Texture";
		param.registerIndex = (U8)type;
		return param;
	}

	bool MaterialGenerator::GenerateSimple(OutputMemoryStream& outmem, MaterialInfo& materialInfo)
	{
		outmem.Write(materialInfo);
		MaterialParams::Save(outmem, params);
		return true;
	}
}
}