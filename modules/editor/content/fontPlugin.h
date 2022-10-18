#pragma once

#include "renderer\render2D\fontResource.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	struct FontPlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		static ImportFileEntry* CreateEntry(const ImportRequest& request);

	public:
		FontPlugin(EditorApp& app_);

		void OnGui(Span<class Resource*> resource)override {}

		ResourceType GetResourceType() const override {
			return FontResource::ResType;
		}
	};
}
}
