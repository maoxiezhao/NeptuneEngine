#include "createMaterial.h"
#include "materialGenerator.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	CreateResult CreateMaterial::Create(CreateResourceContext& ctx)
	{
		IMPORT_SETUP(Material);

		DataChunk* dataChunk = ctx.AllocateChunk(0);
		if (dataChunk == nullptr)
			return CreateResult::AllocateFailed;

		Options* options = (Options*)ctx.customArg;
		MaterialGenerator generator;

		// Write material info
		ctx.WriteCustomData(options->info);

		// Write base propreties
		Color4 baseColor = options->baseColor;
		generator.FindOrCreateEmpty("BaseColor", MaterialParameterType::Color).asColor = baseColor.GetRGBA();
		generator.FindOrCreateEmpty("Metalness", MaterialParameterType::Float).asFloat = options->metalness;
		generator.FindOrCreateEmpty("Roughness", MaterialParameterType::Float).asFloat = options->roughness;
		generator.FindOrCreateEmpty("AlphaRef", MaterialParameterType::Float).asFloat = options->alphaRef;

		// Write textures
		auto WriteTexture = [options, &generator](Texture::TextureType type)
		{
			const auto& path = options->textures[(U32)type];
			if (path.IsEmpty())
				return;

			auto& param = generator.FindOrAddTexture(type);
			param.asPath = path.c_str();
		};
		WriteTexture(Texture::TextureType::DIFFUSE);
		WriteTexture(Texture::TextureType::NORMAL);
		WriteTexture(Texture::TextureType::SPECULAR);

		if (!generator.GenerateSimple(dataChunk->mem, options->info))
			return CreateResult::Error;

		return CreateResult::Ok;
	}
}
}
