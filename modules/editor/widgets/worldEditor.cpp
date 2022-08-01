#include "worldEditor.h"
#include "editor\editor.h"
#include "editor\widgets\gizmo.h"
#include "editor\widgets\entityList.h"

namespace VulkanTest
{
namespace Editor
{
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
            PROFILE_FUNCTION();
            Gizmo::Update();
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

        static void FastRemoveDuplicates(Array<ECS::EntityID>& entities) 
        {
            qsort(entities.begin(), entities.size(), sizeof(entities[0]), [](const void* a, const void* b) {
                return memcmp(a, b, sizeof(ECS::EntityID));
            });

            for (I32 i = entities.size() - 2; i >= 0; --i) 
            {
                if (entities[i] == entities[i + 1]) 
                    entities.swapAndPop(i);
            }
        }

        void SelectEntities(Span<const ECS::EntityID> entities, bool toggle)override
        {
            if (toggle)
            {
                for (auto entity : entities)
                {
                    const I32 index = selectedEntities.indexOf(entity);
                    if (index < 0)
                        selectedEntities.push_back(entity);
                    else
                        selectedEntities.swapAndPop(index);
                }
            }
            else
            {
                selectedEntities.clear();
                for (auto entity : entities)
                    selectedEntities.push_back(entity);
            }

            FastRemoveDuplicates(selectedEntities);
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