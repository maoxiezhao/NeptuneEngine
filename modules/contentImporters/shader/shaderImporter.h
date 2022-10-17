#pragma once

#include "contentImporters\definition.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    namespace ShaderImporter
    {
        CreateResult Import(CreateResourceContext& ctx);
    }
}
}
