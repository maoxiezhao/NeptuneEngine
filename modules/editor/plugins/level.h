#pragma once

#include "editorPlugin.h"

namespace VulkanTest
{
	namespace Editor
	{
		class EditorApp;
		extern "C" EditorPlugin * SetupPluginLevel(EditorApp & app);
	}
}
