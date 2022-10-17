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
		FontPlugin(EditorApp& app_);

		std::vector<const char*> GetSupportExtensions()
		{
			return { "ttf" };
		}

		void OnGui(Span<class Resource*> resource)override {}

		ResourceType GetResourceType() const override {
			return FontResource::ResType;
		}
	};
}
}
