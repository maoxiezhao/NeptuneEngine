#include "createMaterial.h"
#include "materialGenerator.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	CreateResourceContext::CreateResult CreateMaterial::CreateImpl(CreateResourceContext& ctx)
	{
		DataChunk* dataChunk = ctx.writer.GetChunk(0);
		if (dataChunk == nullptr)
			return CreateResourceContext::CreateResult::AllocateFailed;

		Options* options = (Options*)ctx.customArg;
		MaterialGenerator generator;

		// Write material info
		ctx.writer.WriteCustomData(options->info);

		// Write base propreties
		Color4 baseColor = options->baseColor;
		generator.FindOrCreateEmpty("BaseColor", MaterialParameterType::Color).asColor = baseColor.GetRGBA();

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
			return CreateResourceContext::CreateResult::Error;

		return CreateResourceContext::CreateResult::Ok;
	}

	bool CreateMaterial::Create(EditorApp& editor, const Guid& guid, const Path& path, const Options& options)
	{
		Guid targetID = guid;
		if (!targetID.IsValid())
			targetID = Guid::New();

		auto& fs = editor.GetEngine().GetFileSystem();
		auto resFactory = editor.GetEngine().GetResourceManager().GetFactory(Material::ResType);
		ASSERT(resFactory != nullptr);
		auto res = resFactory->GetResource(path);

		// Use the same guid if resource is loaded
		if (res != nullptr)
		{
			targetID = res->GetGUID();
		}
		else
		{
			if (fs.FileExists(path.c_str()))
			{
				// TODO
				// Load target resource and get the guid
			}
		}

		// TODO Use guid for resource index
		CreateResourceContext ctx(editor, targetID, Material::ResType, path.c_str(), (void*)(&options));
		const auto result = ctx.Create(CreateMaterial::CreateImpl);
		if (result == CreateResourceContext::CreateResult::Ok)
		{
			Logger::Info("Create resource %s", path.c_str());
			return true;
		}
		else
		{
			Logger::Info("Faield to create resource %s", path.c_str());
			return false;
		}
	}
}
}
