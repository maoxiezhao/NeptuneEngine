#pragma once

#include "editorModule.h"
#include "widgets\assetItem.h"

namespace VulkanTest
{
namespace Editor
{
	class ThumbnailsModule : public EditorModule
	{
	public:
		ThumbnailsModule(EditorApp& app);
		~ThumbnailsModule();

		virtual void RefreshThumbnail(AssetItem& item) = 0;

		static ThumbnailsModule* Create(EditorApp& app);

	public:
		static GPU::Image* FolderIcon;
		static GPU::Image* SceneIcon;
		static GPU::Image* ShaderIcon;
		static GPU::Image* MaterialIcon;
		static GPU::Image* ModelIcon;
		static GPU::Image* TextureIcon;
		static GPU::Image* FontIcon;
	};
}
}
