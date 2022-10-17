#pragma once

#include "contentImporters\definition.h"
#include "content\resources\texture.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    namespace TextureImporter
    {
        enum class ImageType
        {
            DDS,
            TGA,
            PNG,
            BMP,
            JPEG,
            HDR,
            RAW,
            Internal,
        };
        static bool GetImageType(const char* path, ImageType& type);

        struct ImportConfig
        {
            bool generateMipmaps = true;
            bool compress = false;
        };

        CreateResult Import(CreateResourceContext& ctx);
    }
}
}
