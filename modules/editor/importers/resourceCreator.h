#pragma once

#include "core\resource\resource.h"
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

	class CreateResourceContext
	{
	public:
		EditorApp& editor;
		String input;
		String output;
		void* customArg;
		ResourceInitData initData;
		bool isCompiled;

		Array<DataChunk> chunks;

	public:
		CreateResourceContext(EditorApp& editor_, Guid guid, const String& output_, bool isCompiled, void* arg_);
		~CreateResourceContext();

		CreateResult Create();
		CreateResult Create(const CreateResourceFunction& func);
		DataChunk* AllocateChunk(I32 index);

		template<typename T>
		void WriteCustomData(const T& data_)
		{
			initData.customData.Write(data_);
		}
	};

	struct ResourceCreator
	{
		String tag;
		CreateResourceFunction creator;
	};

#define IMPORT_SETUP(resType) 	ctx.initData.header.type = resType::ResType;
}
}
