#include "worldEditor.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
    EntityFolder::EntityFolder(World& world_) :
        world(world_)
    {
        world.EntityCreated().Bind<&EntityFolder::OnEntityCreated>(this);
        world.EntityDestroyed().Bind<&EntityFolder::OnEntityDestroyed>(this);

        CopyString(rootFolder.name, "root");
    }

    EntityFolder::~EntityFolder()
    {
        world.EntityCreated().Unbind<&EntityFolder::OnEntityCreated>(this);
        world.EntityDestroyed().Unbind<&EntityFolder::OnEntityDestroyed>(this);
    }

    void EntityFolder::MoveToRootFolder(ECS::EntityID entity)
    {
        while (entities.size() <= entity)
            entities.resize(entity * 2.0f);

        FolderEntity& folderEntity = entities[entity];
        folderEntity.next = rootFolder.firstEntity;
        folderEntity.prev = ECS::INVALID_ENTITY;
        rootFolder.firstEntity = entity;

        if (folderEntity.next != ECS::INVALID_ENTITY)
            entities[folderEntity.next].prev = entity;
    }

    ECS::EntityID EntityFolder::GetNextEntity(ECS::EntityID e) const
    {
        return entities[e].next;
    }

    void EntityFolder::OnEntityCreated(ECS::EntityID e)
    {
        MoveToRootFolder(e);
    }

    void EntityFolder::OnEntityDestroyed(ECS::EntityID e)
    {
        FolderEntity& folderEntity = entities[e];
        if (rootFolder.firstEntity == e)
            rootFolder.firstEntity = folderEntity.next;

        if (folderEntity.prev != ECS::INVALID_ENTITY)
            entities[folderEntity.prev].next = folderEntity.next;

        if (folderEntity.next != ECS::INVALID_ENTITY)
            entities[folderEntity.next].prev = folderEntity.prev;

        folderEntity.prev = ECS::INVALID_ENTITY;
        folderEntity.next = ECS::INVALID_ENTITY;
    }

    class WorldEditorImpl : public WorldEditor
    {
    private:
        EditorApp& editor;
        Engine& engine;
        World* world = nullptr;
        LocalPtr<EntityFolder> entityFolder;
        Array<ECS::EntityID> selectedEntities;

    public:
        WorldEditorImpl(EditorApp& editor_) :
            editor(editor_),
            engine(editor_.GetEngine())
        {
            NewWorld();
        }

        ~WorldEditorImpl()
        {
            if (world != nullptr)
                DestroyWorld();
        }

        void Update(F32 dt)override
        {
        }

        void EndFrame()override
        {
        }

        void OnGUI()override
        {
        }

        const char* GetName()override
        {
            return "WorldEditor";
        }

        World* GetWorld()override
        {
            return world;
        }

        void NewWorld()override
        {
            ASSERT(world == nullptr);           
            world = &engine.CreateWorld();
            entityFolder.Create(*world);
        }

        void DestroyWorld()override
        {
            ASSERT(world != nullptr);
            entityFolder.Destroy();
            engine.DestroyWorld(*world);
            world = nullptr;
        }

        EntityFolder& GetEntityFolder()override
        {
            return *entityFolder;
        }

        const Array<ECS::EntityID>& GetSelectedEntities() const override
        {
            return selectedEntities;
        }
    };

    UniquePtr<WorldEditor> WorldEditor::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<WorldEditorImpl>(app);
    }
}
}