#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\filesystem\filesystem.h"
#include "core\resource\resource.h"
#include "core\resource\resourceManager.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API AssetBrowser : public EditorWidget
    {
    public:
        struct VULKAN_EDITOR_API IPlugin
        {
            virtual ~IPlugin() {}

            virtual void OnGui(Span<class Resource*> resource) = 0;
            virtual void OnResourceUnloaded(Resource* resource) = 0;
        };

        static UniquePtr<AssetBrowser> Create(EditorApp& app);
        virtual ~AssetBrowser() {};

        virtual void InitFinished() = 0;
        virtual void AddPlugin(IPlugin& plugin) = 0;
        virtual void RemovePlugin(IPlugin& plugin) = 0;
    };
}
}