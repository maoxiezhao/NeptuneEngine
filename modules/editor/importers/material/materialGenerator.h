#pragma once

#include "editorPlugin.h"
#include "renderer\materials\material.h"

namespace VulkanTest
{
namespace Editor
{
	class MaterialGenerator
	{
	public:
		MaterialGenerator();
		~MaterialGenerator();

		SerializedMaterialParam& FindOrCreateEmpty(const String& name, MaterialParameterType type);
		SerializedMaterialParam& FindOrAddTexture(Texture::TextureType type);

		bool GenerateSimple(OutputMemoryStream& outmem, MaterialInfo& materialInfo);

	private:
		Array<SerializedMaterialParam> params;
	};
}
}
