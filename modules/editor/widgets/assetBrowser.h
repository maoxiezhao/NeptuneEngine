#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class VULKAN_EDITOR_API AssetBrowser : public EditorWidget
    {
    public:
        virtual ~AssetBrowser();

        void Update(F32 dt);
        void EndFrame() override;
        void OnGUI() override;
        const char* GetName();
    };
}
}