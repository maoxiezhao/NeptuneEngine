#pragma once

#ifdef CJING3D_EDITOR

#include "core\common.h"
#include "core\scene\world.h"
#include "core\serialization\iSerializable.h"

namespace VulkanTest
{
    class EntityFolder : public ISerializable
    {
    public:
        using FolderID = U16;
        static constexpr FolderID INVALID_FOLDER = 0xffFF;

        struct Folder
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
        FolderID GetFolder(ECS::Entity e);
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
}

#endif