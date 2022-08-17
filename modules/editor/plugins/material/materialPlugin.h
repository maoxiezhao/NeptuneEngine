#pragma once

#include "editorPlugin.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	struct MaterialPlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		MaterialPlugin(EditorApp& app_);

		bool Compile(const Path& path)override;
		void OnGui(Span<class Resource*> resource)override;

		std::vector<const char*> GetSupportExtensions();
		ResourceType GetResourceType() const override;
	};
}
}
