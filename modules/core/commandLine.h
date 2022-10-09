#pragma once

#include "common.h"
#include "core\utils\string.h"

namespace VulkanTest
{
	class CommandLine
	{
	public:
		struct Options
		{
			bool fullscreen = false;
			bool vsync = true;

#ifdef CJING3D_EDITOR
			bool newProject = false;
			std::string workingPath;
			std::string projectPath;
#endif
		};
		static Options options;
		
		static bool Parse(const char* cmdLine);
	};
}