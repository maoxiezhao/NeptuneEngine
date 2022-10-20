#pragma once

#include "editorModule.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	class LevelModule : public EditorModule
	{
	public:
		LevelModule(EditorApp& app);
		~LevelModule();

		virtual void SaveScene(Scene* scene) = 0;
		virtual void CloseScene(Scene* scene) = 0;

		static LevelModule* Create(EditorApp& app);
	};
}
}
