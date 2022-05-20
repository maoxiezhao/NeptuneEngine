#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\scene\world.h"

namespace VulkanTest
{
    namespace Editor
    {
        class EditorApp;

        class VULKAN_EDITOR_API WorldEditor : public EditorWidget
        {
        public:
            static UniquePtr<WorldEditor> Create(EditorApp& app);

            virtual ~WorldEditor() {};

            virtual World* GetWorld() = 0;
            virtual void NewWorld() = 0;
            virtual void DestroyWorld() = 0;
        };
    }
}