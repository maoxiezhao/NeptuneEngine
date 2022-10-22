#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\filesystem\filesystem.h"
#include "content\resourceManager.h"
#include "assetItem.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    struct MainContentTreeNode;

    class VULKAN_EDITOR_API AssetBrowser : public EditorWidget
    {
    public:
        struct VULKAN_EDITOR_API IPlugin
        {
            virtual ~IPlugin() {}

            virtual void OnGui(Span<class Resource*> resource) = 0;
            virtual void OnResourceUnloaded(Resource* resource) {};
            virtual void Update() {}
            virtual ResourceType GetResourceType() const = 0;
            virtual AssetItem* ConstructItem(const Path& path, const ResourceType& type, const Guid& guid);
            virtual void Open(const AssetItem& item) {}

            virtual bool CreateResource(const Path& path, const char* name) {
                return false;
            }
            virtual bool CreateResourceEnable()const {
                return false;
            }
            virtual const char* GetResourceName() const {
                return nullptr;
            }
        };

        static UniquePtr<AssetBrowser> Create(EditorApp& app);
        virtual ~AssetBrowser() {};

        virtual void OpenInExternalEditor(Resource* resource) = 0;
        virtual void OpenInExternalEditor(const char* path) = 0;
        virtual void AddPlugin(IPlugin& plugin) = 0;
        virtual void RemovePlugin(IPlugin& plugin) = 0;
        virtual IPlugin* GetPlugin(const ResourceType& type) = 0;
        virtual void OnDirectoryEvent(MainContentTreeNode* node, const Path& path, Platform::FileWatcherAction action) = 0;

    public:
        static I32 TileSize;
        static F32 ThumbnailSize;
    };
}
}