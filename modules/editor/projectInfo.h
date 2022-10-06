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

	public:
		ProjectInfo();
		~ProjectInfo();

		bool Save();
		bool Load(const Path& path);
	};
}