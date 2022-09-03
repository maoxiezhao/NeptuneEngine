#pragma once

#include "core\resource\resource.h"

namespace VulkanTest
{
namespace Editor
{
	class CreateResourceContext;
	class EditorApp;

	struct ResourceDataWriter
	{
		ResourceInitData data;
		Array<DataChunk> chunks;

		ResourceDataWriter(ResourceType type_)
		{
			data.header.type = type_;
		}

		template<typename T>
		void WriteCustomData(const T& data_)
		{
			data.customData.Write(data_);
		}

		DataChunk* GetChunk(I32 index)
		{
			ASSERT(index >= 0 && index < MAX_RESOURCE_DATA_CHUNKS);
			DataChunk* chunk = data.header.chunks[index];
			if (chunk == nullptr)
			{
				chunk = &chunks.emplace();
				data.header.chunks[index] = chunk;
			}
			return chunk;
		}
	};

	class CreateResourceContext
	{
	public:
		enum class CreateResult
		{
			Ok,
			Abort,
			Error,
			AllocateFailed,
			SaveFailed,
		};

		typedef std::function<CreateResult(CreateResourceContext&)> CreateResourceFunction;

	public:
		EditorApp& editor;
		String input;
		String output;
		ResourceDataWriter writer;
		void* customArg;

	public:
		CreateResourceContext(EditorApp& editor_, Guid guid, ResourceType type_, const String& output_, void* arg_);
		~CreateResourceContext();

		CreateResult Create(const CreateResourceFunction& func);
	};
}
}
