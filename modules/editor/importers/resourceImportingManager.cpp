#include "resourceImportingManager.h"
#include "editor\editor.h"
#include "core\platform\timer.h"

#include "material\createMaterial.h"
#include "model\modelImporter.h"
#include "texture\textureImporter.h"

namespace VulkanTest
{
namespace Editor
{
	const String ResourceImportingManager::CreateMaterialTag("Material");

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
				{ "raw",  TextureImporter::Import, true },
				{ "png",  TextureImporter::Import, true },
				{ "jpg",  TextureImporter::Import, true },
				{ "tga",  TextureImporter::Import, true },

				// Model
				{ "obj",  ModelImporter::Import, true}
			};
			for (const auto& importer : BuildInImporters)
				ResourceImportingManager::importers.push_back(importer);

			// Creators
			ResourceCreator BuiltInCreators[] = {
				{ ResourceImportingManager::CreateMaterialTag, CreateMaterial::Create },
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

	bool ResourceImportingManager::Create(const String& tag, Guid& guid, const Path& path, void* arg, bool isCompiled)
	{
		const auto creator = GetCreator(tag);
		if (!creator)
		{
			Logger::Warning("Cannot find resource creator for %s", tag);
			return false;
		}
		return Create(creator->creator, guid, path, arg, isCompiled);
	}

	bool ResourceImportingManager::Create(CreateResourceFunction createFunc, Guid& guid, const Path& path, void* arg, bool isCompiled)
	{
		const auto startTime = Timer::GetTimeSeconds();

		if (!guid.IsValid())
			guid = Guid::New();

		auto& fs = Engine::Instance->GetFileSystem();
	
		// Use the same guid if resource is loaded
		ResPtr<Resource> res = ResourceManager::GetResource(path);
		if (res != nullptr)
		{
			guid = res->GetGUID();
		}
		else
		{
			const Path storagePath = ResourceStorage::GetContentPath(path, isCompiled);
			if (fs.FileExists(storagePath.c_str()))
			{
				// Load target resource and get the guid
				auto storage = StorageManager::GetStorage(storagePath, true, isCompiled);
				if (storage)
				{
					auto entry = storage->GetResourceEntry();
					guid = entry.guid;
				}
				else
				{
					Logger::Warning("Cannot open resource storage %s", storagePath.c_str());
				}
			}
		}

		CreateResourceContext ctx(guid, path.c_str(), isCompiled, arg);
		const auto result = ctx.Create(createFunc);

		// Remove ref
		res = nullptr;

		if (result == CreateResult::Ok)
		{
			// check if is resource tile
			if (StartsWith(path.c_str(), ".export/resources_tiles/"))
			{
				const auto endTime = Timer::GetTimeSeconds();
				Logger::Info("Create resource time %s in %.3f s", path.c_str(), endTime - startTime);
				return true;
			}

			// Register if resource is compiled
			ResourceManager::GetCache().Register(ctx.initData.header.guid, ctx.initData.header.type, path);

			const auto endTime = Timer::GetTimeSeconds();
			Logger::Info("Create resource %s in %.3f s", path.c_str(), endTime - startTime);
			return true;
		}
		else
		{
			Logger::Info("Faield to create resource %s", path.c_str());
			return false;
		}
	}

	bool ResourceImportingManager::Import(const Path& path, Guid& resID, void* arg)
	{
		Logger::Info("Importing file %s", path.c_str());
		auto& fs = Engine::Instance->GetFileSystem();
		if (!fs.FileExists(path.c_str()))
		{
			Logger::Error("Missing file %s", path.c_str());
			return false;
		}

		auto extension = Path::GetExtension(path.ToSpan());
		if (extension.length() <= 0)
		{
			Logger::Error("Unknown file extension %s", path.c_str());
			return false;
		}

		const auto importer = GetImporter(extension);
		if (importer == nullptr)
		{
			Logger::Error("Unknown file type %s", path.c_str());
			return false;
		}

		return Create(importer->callback, path, arg, importer->isCompiled);
	}
}
}
