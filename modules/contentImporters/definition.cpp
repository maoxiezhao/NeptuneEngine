#include "definition.h"
#include "content\storage\resourceStorage.h" 
#include "editor\editor.h"
#include "material\createMaterial.h"
#include "core\serialization\jsonWriter.h"
#include "core\threading\mainThreadTask.h"

namespace VulkanTest::Editor
{
	CreateResourceContext::CreateResourceContext(const Guid& guid, const String& input_, const String& output_, void* arg_) :
		input(input_),
		output(output_),
		customArg(arg_),
		skipMetadata(false)
	{
		initData.header.guid = guid;
		tempPath = ResourceManager::CreateTemporaryResourcePath();
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

		// Create a temorary resource to avoiding conflict, then move the file to the target resource path
		ret = ResourceStorage::Create(tempPath, initData) ? CreateResult::Ok : CreateResult::Error;
		if (ret == CreateResult::Ok)
		{
			applyChangeResult = CreateResult::Error;
			INVOKE_ON_MAIN_THREAD(CreateResourceContext, CreateResourceContext::ApplyChanges, this);
			ret = applyChangeResult;
		}
		FileSystem::DeleteFile(tempPath);

		return ret;
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

	void CreateResourceContext::ApplyChanges()
	{
		auto storage = StorageManager::TryGetStorage(Path(output));
		if (storage && storage->IsLoaded())
			storage->CloseContent();
		
		if (!Platform::MoveFile(tempPath, output))
		{
			Logger::Warning("Failed to move imported file %s to %s", tempPath.c_str(), output);
			applyChangeResult = CreateResult::SaveFailed;
			return;
		}

		if (storage)
			storage->Reload();

		applyChangeResult = CreateResult::Ok;
	}
}
