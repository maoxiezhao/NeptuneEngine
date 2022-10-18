#pragma once

#include "content\resources\texture.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	struct TexturePlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;
		Texture* curTexture = nullptr;

	public:
		static ImportFileEntry* CreateEntry(const ImportRequest& request);

	public:
		TexturePlugin(EditorApp& app_);

		void OnGui(Span<class Resource*> resource) override;

		ResourceType GetResourceType() const override {
			return Texture::ResType;
		}
	};
}
}
