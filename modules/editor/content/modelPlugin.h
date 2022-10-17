#pragma once

#include "content\resources\model.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	struct ModelPlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		ModelPlugin(EditorApp& app_);

		void OnGui(Span<class Resource*> resource) override;

		ResourceType GetResourceType() const override {
			return Model::ResType;
		}
	};
}
}
