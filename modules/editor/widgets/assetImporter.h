#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\filesystem\filesystem.h"
#include "content\resource.h"
#include "content\importFileEntry.h"
#include "assetItem.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API AssetImporter : public EditorWidget
    {
    public:
        static UniquePtr<AssetImporter> Create(EditorApp& app);
        virtual ~AssetImporter() {};

        virtual bool Import(const Path& input, const Path& output, bool skipDialog) = 0;
        virtual void Reimport(ResourceItem& item, bool skipDialog) = 0;
        virtual void ShowImportFileDialog(const Path& location) = 0;
    };
}
}