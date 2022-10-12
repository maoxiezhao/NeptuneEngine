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
        Scene
    };
    struct AssetItem
    {
        AssetItemType type = AssetItemType::Resource;
        Path filepath;
        MaxPathString filename;
        MaxPathString shortName;
        GPU::Image* tex = nullptr;
        bool isLoading = false;
    };
}
}