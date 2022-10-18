#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "gpu\vulkan\image.h"

namespace VulkanTest
{
namespace Editor
{
    enum class AssetItemType
    {
        Resource,
        Directory,
        Scritps,
        Scene,
        File
    };

    struct AssetItem
    {
    public:
        AssetItemType type;
        Path filepath;
        MaxPathString filename;
        MaxPathString shortName;
        GPU::Image* tex = nullptr;
        bool isLoading = false;

    public:
        AssetItem(const Path& path, AssetItemType type);

        virtual GPU::Image* DefaultThumbnail()const;
    };

    struct FileItem : public AssetItem
    {
    public:
        FileItem(const Path& path);
    };

    struct ContentFolderItem : public AssetItem
    {
    public:
        ContentFolderItem(const Path& path);

        GPU::Image* DefaultThumbnail()const override;
    };

    struct ResourceItem : public AssetItem
    {
    public:
        Guid id;
        String typeName;
        ResourceType type;

    public:
        ResourceItem(const Path& path, const Guid& id_, const ResourceType& type_);

        GPU::Image* DefaultThumbnail()const override;
    };

}
}