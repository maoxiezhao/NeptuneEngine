#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "editor\widgets\worldEditor.h"
#include "core\serialization\iSerializable.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    class WorldEditor;

    struct VULKAN_EDITOR_API EntityFolder : public ISerializable
    {
        using FolderID = U16;
        static constexpr FolderID INVALID_FOLDER = 0xffFF;

        struct VULKAN_EDITOR_API Folder
        {
            FolderID parentFolder = INVALID_FOLDER;
            FolderID childFolder = INVALID_FOLDER;
            FolderID nextFolder = INVALID_FOLDER;
            FolderID prevFolder = INVALID_FOLDER;
            ECS::Entity firstEntity = ECS::INVALID_ENTITY;
            char name[116];
        };

        struct EntityItem
        {
            FolderID folder = INVALID_FOLDER;
            ECS::Entity next = ECS::INVALID_ENTITY;
            ECS::Entity prev = ECS::INVALID_ENTITY;
        };

        EntityFolder(World& world);
        ~EntityFolder();

        void MoveToFolder(ECS::Entity e, FolderID folderID);
        void RemoveFromFolder(ECS::Entity e, FolderID folderID);
        FolderID EmplaceFolder(FolderID parent);
        void DestroyFolder(FolderID folderID);
        FolderID GetRoot() const { return 0; }
        Folder& GetFolder(FolderID folderID);
        const Folder& GetFolder(FolderID folderID) const;
        ECS::Entity GetNextEntity(ECS::Entity e) const;
        void SelectFolder(FolderID folder) { selectedFolder = folder; }
        FolderID GetSelectedFolder() const { return selectedFolder; }
        Folder& GetRootFolder() {
            return GetFolder(0);
        }

        void Serialize(SerializeStream& stream, const void* otherObj) override;
        void Deserialize(DeserializeStream& stream) override;

    private:
        void OnEntityCreated(ECS::Entity e);
        void OnEntityDestroyed(ECS::Entity e);

        struct FreeList
        {
            Array<Folder> data;
            I32 firstFree = -1;

            FolderID Alloc();
            void Free(FolderID folder);

            Folder& Get(FolderID id) {
                return data[id];
            }
            const Folder& Get(FolderID id) const {
                return data[id];
            }
        };
        FreeList folderPool;
        World& world;
        HashMap<ECS::Entity, EntityItem> entities;
        FolderID selectedFolder;
    };

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
        WorldEditor& worldEditor;
        EntityFolder::FolderID renamingFolder = EntityFolder::INVALID_FOLDER;
        bool folderRenameFocus = false;
        char folderRenameBuf[32];
        char componentFilter[32];

        Array<EntityFolder::FolderID> toCreateFolders;
    };
}
}
