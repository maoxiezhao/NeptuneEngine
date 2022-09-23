#include "jsonResource.h"
#include "storage\storageManager.h"
#include "core\profiler\profiler.h"
#include "core\serialization\json.h"
#include "core\serialization\jsonUtils.h"

namespace VulkanTest
{
	Resource* JsonResourceFactoryBase::NewResource(const ResourceInfo& info)
	{
		return CreateResource(info);
	}

	Resource* JsonResourceFactoryBase::CreateTemporaryResource(const ResourceInfo& info)
	{
		return CreateResource(info);
	}

	JsonResourceBase::JsonResourceBase(const ResourceInfo& info) :
		Resource(info),
		data(nullptr)
	{
	}

	JsonResourceBase::~JsonResourceBase()
	{
	}

	bool JsonResourceBase::IsReady() const
	{
		return Resource::IsReady() && data != nullptr;
	}

	bool JsonResourceBase::LoadResource()
	{
#ifdef CJING3D_EDITOR
		auto fs = ResourceManager::GetFileSystem();
		OutputMemoryStream mem;
		if (!fs->LoadContext(path.c_str(), mem))
			return false;
#else
		auto storage = StorageManager::GetStorage(path, true);
		if (!storage)
			return false;

		ResourceInitData initData;
		if (!storage->LoadResourceHeader(initData))
			return false;

		auto dataChunk = initData.header.chunks[0];
		if (dataChunk == nullptr || !storage->LoadChunk(dataChunk))
			return false;

		OutputMemoryStream& mem = dataChunk->mem;
#endif
		{
			PROFILE_BLOCK("Parse json");
			document.Parse((const char*)mem.Data(), mem.Size());
		}
		if (document.HasParseError())
		{
			Logger::Error("Failed to parse json");
			return false;
		}

		auto guid = GetGUID();
		const Guid resID = JsonUtils::GetGuid(document, "ID");
		if (resID != guid)
		{
			Logger::Warning("Invalid json resource id");
			return false;
		}

		// Get typename
		typeName = JsonUtils::GetString(document, "Typename");

		// Get json resource data
		auto dataMember = document.FindMember("Data");
		if (dataMember == document.MemberEnd())
		{
			Logger::Warning("Missing json resource data.");
			return false;
		}
		data = &dataMember->value;

		return true;
	}

	bool JsonResourceBase::Load()
	{
		return true;
	}

	void JsonResourceBase::Unload()
	{
		ISerializable::SerializeDocument tmp;
		document.Swap(tmp);
		data = nullptr;
	}

	JsonResource::JsonResource(const ResourceInfo& info) :
		JsonResourceBase(info)
	{
	}

	JsonResource::~JsonResource()
	{
	}

	bool JsonResource::LoadResource()
	{
		const bool ret = JsonResourceBase::LoadResource();
		if (ret == false)
			return ret;

		if (!CreateInstance())
			return false;

		return true;
	}

	void JsonResource::Unload()
	{
		DeleteInstance();
		JsonResourceBase::Unload();
	}

	bool JsonResource::CreateInstance()
	{
		return true;
	}

	void JsonResource::DeleteInstance()
	{
	}
}