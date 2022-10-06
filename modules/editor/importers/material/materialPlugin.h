#pragma once

#include "editorPlugin.h"
#include "content\resources\material.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	struct MaterialPlugin final : AssetImporter::IPlugin, AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		MaterialPlugin(EditorApp& app_);

		bool Import(const Path& input, const Path& outptu)override;
		void OnGui(Span<class Resource*> resource)override;

		std::vector<const char*> GetSupportExtensions();
		ResourceType GetResourceType() const override;

	private:
		void SaveMaterial(Material* material);
	};
}
}
