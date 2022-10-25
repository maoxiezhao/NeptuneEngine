#pragma once

#include "editorModule.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;
	struct WorldView;

	class LevelModule : public EditorModule
	{
	public:
		LevelModule(EditorApp& app);
		~LevelModule();

		static LevelModule* Create(EditorApp& app);

		virtual void OpenScene(const Path& path) = 0;
		virtual void OpenScene(const Guid& guid) = 0;
		virtual void CloseScene(Scene* scene) = 0;
		virtual void SaveScene(Scene* scene) = 0;
		virtual void SaveAllScenes() = 0;
		virtual Array<Scene*>& GetLoadedScenes() = 0;
	};
}
}
