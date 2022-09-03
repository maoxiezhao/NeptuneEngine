#include "createResource.h"
#include "editor\editor.h"

namespace VulkanTest::Editor
{
	CreateResourceContext::CreateResourceContext(EditorApp& editor_, Guid guid, ResourceType type_, const String& output_, void* arg_) :
		editor(editor_),
		output(output_),
		writer(type_),
		customArg(arg_)
	{
		writer.data.header.guid = guid;
	}

	CreateResourceContext::~CreateResourceContext()
	{
	}

	CreateResourceContext::CreateResult CreateResourceContext::Create(const CreateResourceFunction& func)
	{
		ASSERT(func != nullptr);
		CreateResult ret = func(*this);
		if (ret != CreateResult::Ok)
			return ret;

		OutputMemoryStream data;
		if (!ResourceStorage::Save(data, writer.data))
		{
			Logger::Error("Failed to save resource storage.");
			return CreateResult::SaveFailed;
		}

		FileSystem& fs = editor.GetEngine().GetFileSystem();
		auto file = fs.OpenFile(output.c_str(), FileFlags::DEFAULT_WRITE);
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
}
