#pragma once

#include "level\sceneResource.h"
#include "editor\widgets\assetBrowser.h"

namespace VulkanTest
{
namespace Editor
{
	struct ScenePlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		ScenePlugin(EditorApp& app_);
		~ScenePlugin();

		void OnGui(Span<class Resource*> resource) override;
		void OnSceneLoaded(Scene* scene, const Guid& sceneID);
		void OnSceneUnloaded(Scene* scene, const Guid& sceneID);
		bool CreateResource(const Path& path, const char* name);
		void DoubleClick(const Path& path);
		void OpenScene(const Path& path);
		void SaveScene(Scene* scene);
		void CloseScene(Scene* scene);

		bool CreateResourceEnable()const {
			return true;
		}

		const char* GetResourceName() const {
			return "Scene";
		}

		ResourceType GetResourceType() const override {
			return SceneResource::ResType;
		}

		bool CheckSaveBeforeClose()
		{
			return true;
		}
	};
}
}
