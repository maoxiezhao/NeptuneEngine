#pragma once

#include "content\resources\material.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	struct MaterialPlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		MaterialPlugin(EditorApp& app_);

		void OnGui(Span<class Resource*> resource)override;

		ResourceType GetResourceType() const override;

	private:
		void SaveMaterial(Material* material);
	};
}
}
