#include "resourceManager.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
	ResourceFactory::ResourceFactory() :
		isUnloadEnable(true),
		resManager(nullptr)
	{
	}

	ResourceFactory::~ResourceFactory()
	{
		ASSERT(resources.empty());
	}

	void ResourceFactory::Initialize(ResourceType type, ResourceManager& resManager_)
	{
		resManager_.RegisterFactory(type, this);
		resType = type;
		resManager = &resManager_;
	}

	void ResourceFactory::Uninitialize()
	{
		for (auto& kvp : resources)
		{
			Resource* res = kvp.second;
			if (!res->IsEmpty())
				Logger::Error("Resource %s leak", res->GetPath().c_str());
			
			DestroyResource(res);
		}
		resources.clear();

		resManager->UnregisterFactory(resType);
		resManager = nullptr;
	}

	Resource* ResourceFactory::LoadResource(const Path& path)
	{
		ASSERT(resManager != nullptr);

		if (path.IsEmpty())
			return nullptr;

		Resource* res = GetResource(path);
		if (res == nullptr)
		{
			res = CreateResource(path);
			resources[path.GetHash()] = res;
		}

		// Do load if resource is empty
		if (res->IsEmpty()) {
			res->DoLoad();
		}

		res->IncRefCount();
		return res;
	}

	Resource* ResourceFactory::GetResource(const Path& path)
	{
		auto it = resources.find(path.GetHash());
		if (it != resources.end())
			return it->second;

		return nullptr;
	}

	ResourceManager::ResourceManager() = default;
	ResourceManager::~ResourceManager() = default;

	void ResourceManager::Initialize(FileSystem& fileSystem_)
	{
		ASSERT(isInitialized == false);
		fileSystem = &fileSystem_;
		isInitialized = true;
	}

	void ResourceManager::Uninitialzie()
	{
		ASSERT(isInitialized == true);
		isInitialized = false;
	}

	Resource* ResourceManager::LoadResource(ResourceType type, const Path& path)
	{
		ASSERT(isInitialized);
		ASSERT(!path.IsEmpty());

		ResourceFactory* factory = GetFactory(type);
		if (factory == nullptr)
			return nullptr;

		return factory->LoadResource(path);
	}

	ResourceFactory* ResourceManager::GetFactory(ResourceType type)
	{
		auto it = factoryTable.find(type.GetHashValue());
		if (it != factoryTable.end())
			return it->second;
		return nullptr;
	}

	void ResourceManager::RegisterFactory(ResourceType type, ResourceFactory* factory)
	{
		ASSERT(isInitialized);
		factoryTable[type.GetHashValue()] = factory;
	}

	void ResourceManager::UnregisterFactory(ResourceType type)
	{
		ASSERT(isInitialized);
		factoryTable.erase(type.GetHashValue());
	}
}