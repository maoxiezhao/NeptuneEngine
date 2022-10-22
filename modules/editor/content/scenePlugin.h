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
		bool CreateResource(const Path& path, const char* name) override;
		void Open(const AssetItem& item) override;
		AssetItem* ConstructItem(const Path& path, const ResourceType& type, const Guid& guid) override;

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
