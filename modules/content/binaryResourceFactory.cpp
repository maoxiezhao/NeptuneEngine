#include "binaryResource.h"

namespace VulkanTest
{
	Resource* BinaryResourceFactoryBase::NewResource(const ResourceInfo& info)
	{
		auto storage = ResourceManager::GetStorage(info.path);
		if (!storage)
		{
			Logger::Warning("Missing resource storage %s", info.path.c_str());
			return nullptr;
		}

		BinaryResource* res = static_cast<BinaryResource*>(CreateResource(info));
		if (res == nullptr)
		{
			Logger::Warning("Faield to create resource %s", info.path.c_str());
			return nullptr;
		}

		ResourceHeader header;
		header.guid = info.guid;
		header.type = info.type;
		if (!res->Initialize(header, storage))
		{
			Logger::Warning("Cannot initialize resource %s", info.path.c_str());
			CJING_DELETE(res);
			return nullptr;
		}

		return res;
	}

	Resource* BinaryResourceFactoryBase::CreateTemporaryResource(const ResourceInfo& info)
	{
		BinaryResource* res = static_cast<BinaryResource*>(CreateResource(info));
		if (res == nullptr)
		{
			Logger::Warning("Faield to create resource %s", info.path.c_str());
			return nullptr;
		}

		ResourceInitData initData;
		initData.header.guid = info.guid;
		initData.header.type = info.type;
		if (!res->Init(initData))
		{
			Logger::Warning("Cannot initialize temporary resource %s", info.path.c_str());
			CJING_DELETE(res);
			return nullptr;
		}

		return res;
	}
}