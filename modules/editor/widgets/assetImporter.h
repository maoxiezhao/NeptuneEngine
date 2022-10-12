#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\filesystem\filesystem.h"
#include "content\resource.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API AssetImporter : public EditorWidget
    {
    public:
        struct VULKAN_EDITOR_API IPlugin
        {
            virtual ~IPlugin() {}
            virtual bool Import(const Path& input, const Path& outptu) = 0;
            virtual std::vector<const char*> GetSupportExtensions() = 0;
        };

        static UniquePtr<AssetImporter> Create(EditorApp& app);
        virtual ~AssetImporter() {};

        virtual void AddPlugin(IPlugin& plugin, const std::vector<const char*>& exts) = 0;
        virtual void AddPlugin(IPlugin& plugin) = 0;
        virtual void RemovePlugin(IPlugin& plugin) = 0;
        virtual ResourceType GetResourceType(const char* path) const = 0;
        virtual void RegisterExtension(const char* extension, ResourceType type) = 0;
        virtual bool Import(const Path& input, const Path& outptu) = 0;
        virtual void ShowImportFileDialog(const Path& location) = 0;
    };
}
}