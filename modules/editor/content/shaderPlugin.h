#pragma once

#include "content\resources\shader.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "importFileEntry.h"

namespace VulkanTest
{
namespace Editor
{
	struct ShaderPlugin final : AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		ShaderPlugin(EditorApp& app_);

		void OnGui(Span<class Resource*> resource) override;

		ResourceType GetResourceType() const override {
			return Shader::ResType;
		}
	};
}
}
