#pragma once

#include "editorPlugin.h"

namespace VulkanTest
{
	namespace Editor
	{
		class EditorApp;

		struct LevelPlugin : EditorPlugin
		{
			virtual void CloseScene(Scene* scene) = 0;
		};
		extern "C" EditorPlugin * SetupPluginLevel(EditorApp & app);
	}
}
