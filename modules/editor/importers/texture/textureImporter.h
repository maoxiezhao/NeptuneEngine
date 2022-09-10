#pragma once

#include "editorPlugin.h"
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

        bool Import(EditorApp& editor_, Guid guid, const char* filename, const ImportConfig& options);
    }
}
}
