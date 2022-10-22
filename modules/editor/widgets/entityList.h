#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\serialization\iSerializable.h"
#include "level\entityFolder.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API EntityListWidget : public EditorWidget
    {
    public:
        explicit EntityListWidget(EditorApp& editor_);
        virtual ~EntityListWidget();

        void Update(F32 dt) override;
        void OnGUI() override;
        const char* GetName();

        EntityFolder::FolderID CreateFolder(EntityFolder::FolderID folderID);
        void RenameEntityFolder(EntityFolder::FolderID folderID, const char* name);
        void ShowCreateEntityGUI(EntityFolder::FolderID folderID);

    private:
        void OnFolderUI(EntityFolder& folders, EntityFolder::FolderID folderID, U32 depth, bool isRoot = false);
        void ShowHierarchy(ECS::Entity entity, const Array<ECS::Entity>& selectedEntities);

    private:
        EditorApp& editor;
        EntityFolder::FolderID renamingFolder = EntityFolder::INVALID_FOLDER;
        bool folderRenameFocus = false;
        char folderRenameBuf[32];
        char componentFilter[32];

        Array<EntityFolder::FolderID> toCreateFolders;
    };
}
}
