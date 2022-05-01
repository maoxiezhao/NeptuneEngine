#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class VULKAN_EDITOR_API AssetCompiler : public EditorWidget
    {
    public:
        virtual ~AssetCompiler();

        void Update(F32 dt);
        void EndFrame() override;
        void OnGUI() override;
        const char* GetName();
    };
}
}