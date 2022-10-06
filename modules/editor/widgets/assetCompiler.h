#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\filesystem\filesystem.h"
#include "content\resource.h"
#include "core\scripts\luaUtils.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

#if 0
    class VULKAN_EDITOR_API AssetCompiler : public EditorWidget
    {
    public:
        struct VULKAN_EDITOR_API IPlugin
        {
            virtual ~IPlugin() {}
            virtual bool Compile(const Path& path, Guid guid) = 0;
            virtual void RegisterResource(AssetCompiler& compiler, const char* path);
            virtual std::vector<const char*> GetSupportExtensions() = 0;
        };

        struct ResourceItem 
        {
            Path path;
            ResourceType type;
            RuntimeHash dirHash;
        };

        static UniquePtr<AssetCompiler> Create(EditorApp& app);

        virtual ~AssetCompiler() {};

        virtual void AddPlugin(IPlugin& plugin, const std::vector<const char*>& exts) = 0;
        virtual void AddPlugin(IPlugin& plugin) = 0;
        virtual void RemovePlugin(IPlugin& plugin) = 0;
        virtual void AddDependency(const Path& from, const Path& dep) = 0;
        virtual bool Compile(const Path& path, Guid guid) = 0;
        virtual ResourceType GetResourceType(const char* path) const = 0;
        virtual void RegisterExtension(const char* extension, ResourceType type) = 0;
        virtual void AddResource(ResourceType type, const char* path) = 0;
        virtual const HashMap<U64, ResourceItem>& LockResources() = 0;
        virtual void UnlockResources() = 0;
        virtual DelegateList<void(const Path& path)>& GetListChangedCallback() = 0;
        virtual void UpdateMeta(const Path& path, const char* src) const = 0;
        virtual bool GetMeta(const Path& path, void* userPtr, void (*callback)(void*, lua_State*)) const = 0;

        template <typename T>
        bool GetMeta(const Path& path, T callback) {
            return GetMeta(path, &callback, [](void* userPtr, lua_State* L) {
                return (*(T*)userPtr)(L);
            });
        }
    };
#endif
}
}