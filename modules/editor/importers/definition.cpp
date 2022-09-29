#include "definition.h"
#include "editor\editor.h"
#include "editor\importers\material\createMaterial.h"

namespace VulkanTest::Editor
{
	CreateResourceContext::CreateResourceContext(const Guid& guid, const String& output_, bool isCompiled_, void* arg_) :
		input(output_),
		output(output_),
		isCompiled(isCompiled_),
		customArg(arg_)
	{
		initData.header.guid = guid;
	}

	CreateResourceContext::~CreateResourceContext()
	{
	}

	CreateResult CreateResourceContext::Create(const CreateResourceFunction& func)
	{
		ASSERT(func != nullptr);
		CreateResult ret = func(*this);
		if (ret != CreateResult::Ok)
			return ret;

		OutputMemoryStream data;
		if (!ResourceStorage::Save(data, initData))
		{
			Logger::Error("Failed to save resource storage.");
			return CreateResult::SaveFailed;
		}

		FileSystem& fs = Engine::Instance->GetFileSystem();
		Path outputPath(output.c_str());
		auto storage = StorageManager::TryGetStorage(outputPath, isCompiled);

		bool needReload = false;
		if (storage && storage->IsLoaded())
		{
			storage->CloseContent();
			needReload = true;
		}

		Path storagePath = ResourceStorage::GetContentPath(outputPath, isCompiled);
		auto file = fs.OpenFile(storagePath.c_str(), FileFlags::DEFAULT_WRITE);
		if (!file)
		{
			Logger::Error("Failed to create resource file %s", output.c_str());
			return CreateResult::Error;
		}
		if (!file->Write(data.Data(), data.Size()))
			Logger::Error("Failed to write mat file %s", output.c_str());

		file->Close();

		//if (storage && needReload)
		//	storage->Reload();

		return CreateResult::Ok;
	}

	DataChunk* CreateResourceContext::AllocateChunk(I32 index)
	{
		if (index < 0 || index >= MAX_RESOURCE_DATA_CHUNKS)
		{
			Logger::Warning("Invalid resource chunk index %d.", index);
			return nullptr;
		}

		// Check if chunk has already allocated
		if (initData.header.chunks[index] != nullptr)
		{
			Logger::Warning("Resource chunk %d has been already allocated.", index);
			return nullptr;
		}

		auto chunk = &chunks.emplace();
		initData.header.chunks[index] = chunk;
		return chunk;
	}
}
