#include "definition.h"
#include "editor\editor.h"
#include "editor\importers\material\createMaterial.h"
#include "core\serialization\jsonWriter.h"

namespace VulkanTest::Editor
{
	CreateResourceContext::CreateResourceContext(const Guid& guid, const String& input_, const String& output_, void* arg_) :
		input(input_),
		output(output_),
		customArg(arg_),
		skipMetadata(false)
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

		// Skip for non-res resource, like json resource or custom resource type
		if (!EndsWith(output.c_str(), RESOURCE_FILES_EXTENSION))
			return CreateResult::Ok;

		// Add import metadata
		if (!skipMetadata && initData.metadata.Empty())
		{
			rapidjson_flax::StringBuffer buffer;
			JsonWriter writer(buffer);
			writer.StartObject();
			{
				writer.JKEY("ImportPath");
				writer.String(input);
			}
			writer.EndObject();
			initData.metadata.Write(buffer.GetString(), buffer.GetSize());
		}

		// Save resource file
		OutputMemoryStream data;
		if (!ResourceStorage::Save(data, initData))
		{
			Logger::Error("Failed to save resource storage.");
			return CreateResult::SaveFailed;
		}

		Path outputPath(output.c_str());
		bool needReload = false;
		auto storage = StorageManager::TryGetStorage(outputPath);
		if (storage && storage->IsLoaded())
		{
			storage->CloseContent();
			needReload = true;
		}

		auto file = FileSystem::OpenFile(outputPath.c_str(), FileFlags::DEFAULT_WRITE);
		if (!file)
		{
			Logger::Error("Failed to create resource file %s", output.c_str());
			return CreateResult::Error;
		}
		if (!file->Write(data.Data(), data.Size()))
			Logger::Error("Failed to write mat file %s", output.c_str());

		file->Close();

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
