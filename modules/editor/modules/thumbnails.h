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
	};
}
}
