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

    class VULKAN_EDITOR_API AssetCompiler : public EditorWidget
    {
    public:
        struct VULKAN_EDITOR_API IPlugin
        {
            virtual ~IPlugin() {}
            virtual bool Compile(const Path& path) = 0;
        };

        struct ResourceItem 
        {
            Path path;
            ResourceType type;
            RuntimeHash dirHash;
        };

        static UniquePtr<AssetCompiler> Create(EditorApp& app);

        virtual ~AssetCompiler() {};

        virtual void InitFinished() = 0;
        virtual void AddPlugin(IPlugin& plugin, const char* ext) = 0;
        virtual void RemovePlugin(IPlugin& plugin) = 0;
        virtual bool Compile(const Path& path) = 0;
        virtual bool WriteCompiled(const char* path, Span<const U8>data) = 0;
    };
}
}