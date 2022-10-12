#pragma once

#include "editorPlugin.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ProjectInfo
	{
	public:
		String Name;
		Path ProjectPath;
		Path ProjectFolderPath;
		U32 Version;
		static U32 PROJECT_VERSION;

		struct Reference
		{
			String Name;
			ProjectInfo* Project = nullptr;
		};
		Array<Reference> References;

	public:
		static Array<ProjectInfo*> ProjectsCache;
		static ProjectInfo* EditorProject;
		static ProjectInfo* EngineProject;

	public:
		ProjectInfo();
		~ProjectInfo();

		bool Save();
		bool Load(const Path& path);
		static ProjectInfo* LoadProject(const Path& path);
	};
}