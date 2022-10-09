#include "projectInfo.h"
#include "core\serialization\json.h"
#include "core\serialization\jsonUtils.h"

namespace VulkanTest
{
	U32 ProjectInfo::PROJECT_VERSION = 01;
	Array<ProjectInfo*> ProjectInfo::ProjectsCache;
	ProjectInfo* ProjectInfo::EditorProject = nullptr;

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

			stream.JKEY("References");
			stream.StartArray();
			for (auto& ref : References)
			{
				stream.StartObject();
				stream.JKEY("Name");
				stream.String(ref.Name);
				stream.EndObject();
			}
			stream.EndArray();
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

		char projectFolder[MAX_PATH_LENGTH];
		CopyString(projectFolder, Path::GetDir(path));
		ProjectFolderPath = projectFolder;
		U32 version = JsonUtils::GetUint(document, "Version", 0);
		if (version != PROJECT_VERSION)
		{
			Logger::Error("Invalid project version %s", path.c_str());
			return false;
		}

		auto refIt = document.FindMember("References");
		if (refIt != document.MemberEnd())
		{
			auto& refDatas = refIt->value;
			References.resize(refDatas.Size());
			for (int i = 0; i < refDatas.Size(); i++)
			{
				auto& ref = References[i];
				auto& refData = refDatas[i];
				ref.Name = JsonUtils::GetString(refData, "Name");

				Path refPath;
				if (StartsWith(ref.Name.c_str(), "$(EnginePath)"))
				{
					refPath = Globals::StartupFolder / ref.Name.substr(StringLength("$(EnginePath)"));
				}
				else if (Path(ref.Name).IsRelative())
				{
					refPath = Globals::StartupFolder / ref.Name;
				}
				else
				{
					refPath = ref.Name;
				}

				ref.Project = LoadProject(refPath);
				if (ref.Project == nullptr)
				{
					Logger::Error("Failed to load reference project %s", refPath.c_str());
					return false;
				}
			}
		}

		return true;
	}

	ProjectInfo* ProjectInfo::LoadProject(const Path& path)
	{
		for (auto& project : ProjectsCache)
		{
			if (project->ProjectPath == path)
				return project;
		}

		auto project = CJING_NEW(ProjectInfo)();
		if (!project->Load(path))
		{
			CJING_SAFE_DELETE(project);
			return nullptr;
		}

		ProjectsCache.push_back(project);
		return project;
	}
}