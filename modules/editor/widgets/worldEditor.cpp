#include "worldEditor.h"
#include "editor\editor.h"
#include "editor\widgets\gizmo.h"
#include "editor\widgets\entityList.h"
#include "core\utils\stateMachine.h"
#include "level\level.h"

namespace VulkanTest
{
namespace Editor
{
    namespace
    {
        struct AddEmptyEntityCommand final : public IEditorCommand
        {
            AddEmptyEntityCommand(WorldEditor& worldEditor_, ECS::Entity* output_) :
                worldEditor(worldEditor_),
                entity(ECS::INVALID_ENTITY),
                output(output_)
            {
            }

            bool Execute() override
            {
                ASSERT(entity == ECS::INVALID_ENTITY);
                entity = worldEditor.GetWorld()->CreateEntity(nullptr);

                if (output)
                    *output = entity;
                return true;
            }

            void Undo() override
            {
                ASSERT(entity);
                worldEditor.GetWorld()->DeleteEntity(entity);
            }

            const char* GetType() override {
                return "AddEmtpyEntity";
            }

        private:
            WorldEditor& worldEditor;
            ECS::Entity entity;
            ECS::Entity* output;
        };

        struct DeleteEntityCommand final : public IEditorCommand
        {
            DeleteEntityCommand(WorldEditor& worldEditor_, ECS::Entity entity_) :
                worldEditor(worldEditor_),
                entity(entity_)
            {
            }

            bool Execute() override
            {
                ASSERT(entity != ECS::INVALID_ENTITY);
                worldEditor.GetWorld()->DeleteEntity(entity);
                return true;
            }

            void Undo() override
            {
                ASSERT(false);
            }

            const char* GetType() override {
                return "DeleteEntity";
            }

        private:
            WorldEditor& worldEditor;
            ECS::Entity entity;
        };

        struct AddComponentCommand final : public IEditorCommand
        {
            AddComponentCommand(WorldEditor& worldEditor_, ECS::Entity entity_, ECS::EntityID compID_) :
                worldEditor(worldEditor_),
                entity(ECS::INVALID_ENTITY),
                compID(compID_)
            {
                if (!entity_.Has(compID))
                    entity = entity_;
            }

            bool Execute() override
            {
                ASSERT(entity != ECS::INVALID_ENTITY);
                ASSERT(compID != ECS::INVALID_ENTITY);
                ASSERT(!entity.Has(compID));

                RenderScene* scene = dynamic_cast<RenderScene*>(worldEditor.GetWorld()->GetScene("Renderer"));
                if (!scene)
                    return false;

                scene->CreateComponent(entity, compID);

                return entity.Has(compID);
            }

            void Undo() override
            {
            }

            const char* GetType() override {
                return "AddComponent";
            }

        private:
            WorldEditor& worldEditor;
            ECS::Entity entity;
            ECS::EntityID compID;
        };
    }

    class WorldEditorImpl : public WorldEditor
    {
    private:
        EditorApp& editor;
        Engine& engine;
        World* world = nullptr;
        WorldView* view = nullptr;
        LocalPtr<EntityFolder> entityFolder;
        Array<ECS::Entity> selectedEntities;
        Array<Guid> toLoadScenes;
        Guid lastRequestScene = Guid::Empty;

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
            editor.OnWorldChanged(world);
        }

        void DestroyWorld()override
        {
            ASSERT(world != nullptr);
            entityFolder.Destroy();
            engine.DestroyWorld(*world);
            selectedEntities.clear();
            world = nullptr;
        }

        void ExecuteCommand(UniquePtr<IEditorCommand>&& command) override
        {
            if (!command->Execute())
                Logger::Error("Failed to execute editor command: %s", command->GetType());

            command.Reset();
        }

        ECS::Entity AddEmptyEntity() override
        {
            ECS::Entity entity;
            UniquePtr<AddEmptyEntityCommand> command = CJING_MAKE_UNIQUE<AddEmptyEntityCommand>(*this, &entity);
            ExecuteCommand(std::move(command));
            return entity;
        }

        ECS::Entity DeleteEntity(ECS::Entity entity) override
        {
            UniquePtr<DeleteEntityCommand> command = CJING_MAKE_UNIQUE<DeleteEntityCommand>(*this, entity);
            ExecuteCommand(std::move(command));
            return entity;
        }

        void AddComponent(ECS::Entity entity, ECS::EntityID compID) override
        {
            UniquePtr<AddComponentCommand> command = CJING_MAKE_UNIQUE<AddComponentCommand>(*this, entity, compID);
            ExecuteCommand(std::move(command));
        }

        WorldView& GetView() override
        {
            return *view;
        }
       
        virtual void SetView(WorldView& view_) override
        {
            view = &view_;
        }

        bool CanChangeScene() override
        {
            return true;
        }

        // TODO
        // Use state machine to manager scenes

        void SetWorldTest(World* world_)
        {
            DestroyWorld();
            world = world_;
            editor.OnWorldChanged(world);
        }

        void TryEnterScene()
        {
            // Skip loaded scenes
            for (int i = 0; i < toLoadScenes.size(); i++)
            {
                if (Level::FindScene(toLoadScenes[i]) != nullptr)
                {
                    toLoadScenes.swapAndPopItem(toLoadScenes[i]);
                    if (toLoadScenes.size() == 0)
                        break;
                    i--;
                }
            }

            // Load scenes
            for (const auto& sceneID : toLoadScenes)
            {
                if (Level::LoadScene(sceneID))
                    lastRequestScene = sceneID;
            }

            if (lastRequestScene == Guid::Empty)
            {
                Logger::Warning("Cannot open scene %d", lastRequestScene);
                return;
            }

            // Test
            DestroyWorld();
            NewWorld();
        }

        void LoadScene(const Guid& guid) override
        {
            toLoadScenes.clear();
            toLoadScenes.push_back(guid);
            TryEnterScene();
        }

        static void FastRemoveDuplicates(Array<ECS::Entity>& entities)
        {
            qsort(entities.begin(), entities.size(), sizeof(entities[0]), [](const void* a, const void* b) {
                return memcmp(a, b, sizeof(ECS::Entity));
                });

            for (I32 i = entities.size() - 2; i >= 0; --i)
            {
                if (entities[i] == entities[i + 1])
                    entities.swapAndPop(i);
            }
        }

        void ClearSelectEntities() override
        {
            selectedEntities.clear();
        }

        void SelectEntities(Span<const ECS::Entity> entities, bool toggle)override
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

        const Array<ECS::Entity>& GetSelectedEntities() const override
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