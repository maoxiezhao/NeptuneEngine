#include "resourceImportingManager.h"
#include "editor\editor.h"
#include "core\platform\timer.h"

#include "material\createMaterial.h"
#include "model\modelImporter.h"
#include "texture\textureImporter.h"
#include "shader\shaderImporter.h"
#include "font\fontImporter.h"

namespace VulkanTest
{
namespace Editor
{
	const String ResourceImportingManager::CreateMaterialTag("Material");
	const String ResourceImportingManager::CreateModelTag("Model");
	
	Array<ResourceImporter> ResourceImportingManager::importers;
	Array<ResourceCreator> ResourceImportingManager::creators;

	class ResourceImportingManagerServiceImpl : public EngineService
	{
	public:
		ResourceImportingManagerServiceImpl() :
			EngineService("ResourceImportingManagerService", -400)
		{}

		bool Init(Engine& engine) override
		{
			// Importers
			ResourceImporter BuildInImporters[] = {

				// Texture
				{ "raw",  TextureImporter::Import },
				{ "png",  TextureImporter::Import },
				{ "jpg",  TextureImporter::Import },
				{ "tga",  TextureImporter::Import },

				// Model
				{ "obj",  ModelImporter::Import },
				{ "gltf", ModelImporter::Import },
				{ "glb",  ModelImporter::Import },

				// Shader
				{ "shd",  ShaderImporter::Import },

				// Font
				{ "ttf",  FontImporter::Import },
			};
			for (const auto& importer : BuildInImporters)
				ResourceImportingManager::importers.push_back(importer);

			// Creators
			ResourceCreator BuiltInCreators[] = {
				{ ResourceImportingManager::CreateMaterialTag, CreateMaterial::Create },
				{ ResourceImportingManager::CreateModelTag, ModelImporter::Create },
			};
			for (const auto& creator : BuiltInCreators)
				ResourceImportingManager::creators.push_back(creator);

			initialized = true;
			return true;
		}

		void Uninit() override
		{
			ResourceImportingManager::importers.clear();
			ResourceImportingManager::creators.clear();
		}
	};
	ResourceImportingManagerServiceImpl ResourceImportingManagerServiceImplInstance;

	const ResourceImporter* ResourceImportingManager::GetImporter(const String& extension)
	{
		for (auto& importer : importers)
		{
			if (importer.extension == extension)
				return &importer;
		}
		return nullptr;
	}

	const ResourceCreator* ResourceImportingManager::GetCreator(const String& tag)
	{
		for (auto& creator : creators)
		{
			if (creator.tag == tag)
				return &creator;
		}
		return nullptr;
	}

	bool ResourceImportingManager::Create(const String& tag, Guid& guid, const Path& targetPath, void* arg)
	{
		const auto creator = GetCreator(tag);
		if (!creator)
		{
			Logger::Warning("Cannot find resource creator for %s", tag);
			return false;
		}
		return Create(creator->creator, guid, targetPath, targetPath, arg);
	}

	bool ResourceImportingManager::Create(CreateResourceFunction createFunc, Guid& guid, const Path& inputPath, const Path& outputPath, void* arg)
	{
		const auto startTime = Timer::GetTimeSeconds();

		if (!guid.IsValid())
			guid = Guid::New();
	
		// Use the same guid if resource is loaded
		ResPtr<Resource> res = ResourceManager::GetResource(outputPath);
		if (res != nullptr)
		{
			guid = res->GetGUID();
		}
		else
		{
			if (FileSystem::FileExists(outputPath))
			{
				// Load target resource and get the guid
				auto storage = StorageManager::GetStorage(outputPath, true);
				if (storage)
				{
					auto entry = storage->GetResourceEntry();
					guid = entry.guid;
				}
				else
				{
					Logger::Warning("Cannot open resource storage %s", outputPath.c_str());
				}
			}
			else
			{
				const String outputDirectory = Path::GetDir(outputPath);
				if (!Platform::DirExists(outputDirectory.c_str()))
					Platform::MakeDir(outputDirectory.c_str());
			}
		}

		CreateResourceContext ctx(guid, inputPath.c_str(), outputPath.c_str(), arg);
		const auto result = ctx.Create(createFunc);

		// Remove ref
		res = nullptr;

		if (result == CreateResult::Ok)
		{
			// Register if resource is compiled
			ResourceManager::GetCache().Register(ctx.initData.header.guid, ctx.initData.header.type, outputPath);

			const auto endTime = Timer::GetTimeSeconds();
			Logger::Info("Create resource %s in %.3f s", inputPath.c_str(), endTime - startTime);
			return true;
		}
		else
		{
			Logger::Info("Faield to create resource %s", inputPath.c_str());
			return false;
		}
	}

	bool ResourceImportingManager::Import(const Path& inputPath, const Path& outputPath, Guid& resID, void* arg)
	{
		Logger::Info("Importing file %s", inputPath.c_str());
		if (!FileSystem::FileExists(inputPath.c_str()))
		{
			Logger::Error("Missing file %s", inputPath.c_str());
			return false;
		}

		auto extension = Path::GetExtension(inputPath.ToSpan());
		if (extension.length() <= 0)
		{
			Logger::Error("Unknown file extension %s", inputPath.c_str());
			return false;
		}

		const auto importer = GetImporter(extension);
		if (importer == nullptr)
		{
			Logger::Error("Unknown file type %s", inputPath.c_str());
			return false;
		}

		return Create(importer->callback, resID, inputPath, outputPath, arg);
	}

	bool ResourceImportingManager::ImportIfEdited(const Path& inputPath, const Path& outputPath, Guid& resID, void* arg)
	{
		if (!FileSystem::FileExists(outputPath))
			return Import(inputPath, outputPath, resID, arg);

		U64 srcTime = FileSystem::GetLastModTime(inputPath);
		U64 dstTime = FileSystem::GetLastModTime(outputPath);
		if (srcTime > dstTime)
			return Import(inputPath, outputPath, resID, arg);

		if (!resID.IsValid())
		{
			ResourceInfo info;
			ResourceManager::GetResourceInfo(outputPath, info);
			resID = info.guid;
		}
		return true;
	}
}
}
