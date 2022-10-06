#include "projectInfo.h"
#include "core\serialization\json.h"
#include "core\serialization\jsonUtils.h"

namespace VulkanTest
{
	U32 ProjectInfo::PROJECT_VERSION = 01;

	ProjectInfo::ProjectInfo() :
		Version(ProjectInfo::PROJECT_VERSION)
	{
	}

	ProjectInfo::~ProjectInfo()
	{
	}

	bool ProjectInfo::Save()
	{
		rapidjson_flax::StringBuffer buffer;
		JsonWriter stream(buffer);
		stream.StartObject();
		{
			stream.JKEY("Name");
			stream.String(Name);

			stream.JKEY("Version");
			stream.Uint(Version);
		}
		stream.EndObject();

		auto file = FileSystem::OpenFile(ProjectPath.c_str(), FileFlags::DEFAULT_WRITE);
		if (!file)
		{
			Logger::Error("Failed to save project %s", ProjectPath.c_str());
			return false;
		}
		if (!file->Write(buffer.GetString(), buffer.GetSize()))
		{
			Logger::Error("Failed to save project %s", ProjectPath.c_str());
			return false;
		}
		file->Close();
		return true;
	}

	bool ProjectInfo::Load(const Path& path)
	{
		OutputMemoryStream mem;
		if (!FileSystem::LoadContext(path, mem))
		{
			Logger::Error("Failed to load project %s", path.c_str());
			return false;
		}

		rapidjson_flax::Document document;
		document.Parse((const char*)mem.Data(), mem.Size());
		if (document.HasParseError())
		{
			Logger::Error("Failed to load project %s", path.c_str());
			return false;
		}

		// Parse json
		Name = JsonUtils::GetString(document, "Name");
		ProjectPath = path;
		ProjectFolderPath = Path(Path::GetDir(path).data());
		U32 version = JsonUtils::GetUint(document, "Version", 0);
		if (version != PROJECT_VERSION)
		{
			Logger::Error("Invalid project version %s", path.c_str());
			return false;
		}

		return true;
	}
}