#pragma once

#include "editorPlugin.h"
#include "editor\importers\definition.h"

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
