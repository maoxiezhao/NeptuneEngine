#pragma once

#include "content\resource.h"
#include "core\utils\string.h"

namespace VulkanTest
{
namespace Editor
{
	class CreateResourceContext;
	class EditorApp;

	enum class CreateResult
	{
		Ok,
		Abort,
		Error,
		AllocateFailed,
		SaveFailed,
	};

	typedef std::function<CreateResult(CreateResourceContext&)> CreateResourceFunction;

	struct ResourceImporter
	{
		String extension;
		CreateResourceFunction callback;
	};

	struct ResourceCreator
	{
		String tag;
		CreateResourceFunction creator;
	};

	class CreateResourceContext
	{
	public:
		String input;
		String output;
		Path tempPath;
		void* customArg;
		ResourceInitData initData;
		bool skipMetadata;
		Array<DataChunk> chunks;
		CreateResult applyChangeResult;

	public:
		CreateResourceContext(const Guid& guid, const String& input_, const String& output_, void* arg_);
		~CreateResourceContext();

		CreateResult Create();
		CreateResult Create(const CreateResourceFunction& func);
		DataChunk* AllocateChunk(I32 index);

		template<typename T>
		void WriteCustomData(const T& data_)
		{
			initData.customData.Write(data_);
		}

		void ApplyChanges();
	};

#define IMPORT_SETUP(resType) 	ctx.initData.header.type = resType::ResType;
}
}
