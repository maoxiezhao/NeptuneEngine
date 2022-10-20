#include "worldEditor.h"
#include "editor\editor.h"
#include "editor\widgets\gizmo.h"
#include "editor\widgets\entityList.h"
#include "modules\level.h"
#include "core\utils\stateMachine.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\jsonUtils.h"

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
            AddComponentCommand(WorldEditor& worldEditor_, ECS::Entity entity_, ComponentType compType_) :
                worldEditor(worldEditor_),
                entity(entity_),
                compType(compType_)
            {
            }

            bool Execute() override
            {
                ASSERT(entity != ECS::INVALID_ENTITY);
                ASSERT(compType != INVALID_COMPONENT_TYPE);

                RenderScene* scene = dynamic_cast<RenderScene*>(worldEditor.GetWorld()->GetScene("Renderer"));
                if (!scene)
                    return false;

                scene->CreateComponent(entity, compType);
                return true;
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
            ComponentType compType;
        };

        struct MakeParentCommand final : public IEditorCommand
        {
            MakeParentCommand(WorldEditor& worldEditor_, ECS::Entity parent_, ECS::Entity child_) :
                worldEditor(worldEditor_),
                parent(parent_),
                child(child_)
            {
                oldFolder = worldEditor.GetEntityFolder()->GetFolder(child);
                oldParent = child_.GetParent();
            }

            bool Execute() override
            {
                ASSERT(child != ECS::INVALID_ENTITY);
                child.ChildOf(parent);
                if (parent != ECS::INVALID_ENTITY)
                {
                    auto folder = worldEditor.GetEntityFolder()->GetFolder(parent);
                    worldEditor.GetEntityFolder()->MoveToFolder(child, folder);
                }
                return true;
            }

            void Undo() override
            {
            }

            const char* GetType() override {
                return "MakeParent";
            }

        private:
            WorldEditor& worldEditor;
            ECS::Entity parent;
            ECS::Entity child;
            EntityFolder::FolderID oldFolder;
            ECS::Entity oldParent;
        };
    }

    class WorldEditorImpl : public WorldEditor
    {
    private:
        EditorApp& editor;
        Engine& engine;
        Array<Scene*> scenes;
        Scene* editingScene = nullptr;
        Scene* nextEditingScene = nullptr;
        WorldView* view = nullptr;
        HashMap<Guid, EntityFolder*> entityFolderMap;
        Array<ECS::Entity> selectedEntities;

    public:
        WorldEditorImpl(EditorApp& editor_) :
            editor(editor_),
            engine(editor_.GetEngine())
        {
            Level::SceneSerializing.Bind<&WorldEditorImpl::OnSceneSerializing>(this);
            Level::sceneDeserializing.Bind<&WorldEditorImpl::OnSceneDeserializing>(this);
        }

        ~WorldEditorImpl()
        {
            for (auto folder : entityFolderMap)
                CJING_SAFE_DELETE(folder);
        }

        void Update(F32 dt)override
        {
            PROFILE_FUNCTION();
            Gizmo::Update();
        }

        void EndFrame()override
        {
            if (nextEditingScene != nullptr)
            {
                SetEditingSceneImpl(nextEditingScene);
                nextEditingScene = nullptr;
            }
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
            return editingScene != nullptr ? editingScene->GetWorld() : nullptr;
        }

        Scene* GetEditingScene()override
        {
            return editingScene;
        }

        void OnSceneSerializing(ISerializable::SerializeStream& data, Scene* scene)
        {
            auto it = entityFolderMap.find(scene->GetGUID());
            if (!it.isValid())
                return;

            data.JKEY("EntityFolders");
            it.value()->Serialize(data, nullptr);
        }

        void OnSceneDeserializing(ISerializable::DeserializeStream* data, Scene* scene)
        {
            auto dataIt = data->FindMember("EntityFolders");
            if (dataIt == data->MemberEnd())
                return;

            auto it = entityFolderMap.find(scene->GetGUID());
            if (!it.isValid())
            {
                EntityFolder* folder = CJING_NEW(EntityFolder)(*scene->GetWorld());
                entityFolderMap.insert(scene->GetGUID(), folder);
                folder->Deserialize(dataIt->value);
            }
        }

        void SetEditingSceneImpl(Scene* scene_)
        {
            if (scene_ == editingScene)
                return;

            if (editingScene != nullptr)
                selectedEntities.clear();

            Scene* prevScene = editingScene;
            editingScene = scene_;

            // Notify editing scene changed
            editor.OnEditingSceneChanged(scene_, prevScene);
        }

        void SetEditingScene(Scene* scene_)override
        {
            if (scene_ == editingScene)
                return;

            nextEditingScene = scene_;
        }

        Array<Scene*>& GetScenes() override
        {
            return scenes;
        }

        void OpenScene(Scene* scene_) override
        {
            ASSERT(scene_);
            for (auto scene : scenes)
            {
                if (scene == scene_)
                {
                    SetEditingScene(scene);
                    return;
                }
            }

            scenes.push_back(scene_);

            auto it = entityFolderMap.find(scene_->GetGUID());
            if (!it.isValid())
            {
                EntityFolder* folder = CJING_NEW(EntityFolder)(*scene_->GetWorld());
                entityFolderMap.insert(scene_->GetGUID(), folder);
            }
            
            SetEditingScene(scene_);
        }
        
        void CloseScene(Scene* scene_) override
        {
            ASSERT(scene_);
            scenes.erase(scene_);
            auto it = entityFolderMap.find(scene_->GetGUID());
            if (it.isValid())
            {
                CJING_SAFE_DELETE(it.value());
                entityFolderMap.erase(it);
            }

            if (editingScene == scene_ && !scenes.empty())
                SetEditingScene(scenes.back());
            else
                SetEditingSceneImpl(nullptr);
        }

        void SaveEditingScene() override
        {
            if (editingScene == nullptr)
                return;

            editor.GetLevelModule().SaveScene(editingScene);
        }

        void SaveAllScenes() override
        {
            auto& levelModule = editor.GetLevelModule();
            for (auto scene : scenes)
                levelModule.SaveScene(scene);
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

        void AddComponent(ECS::Entity entity, ComponentType compType) override
        {
            UniquePtr<AddComponentCommand> command = CJING_MAKE_UNIQUE<AddComponentCommand>(*this, entity, compType);
            ExecuteCommand(std::move(command));
        }

        void MakeParent(ECS::Entity parent, ECS::Entity child) override
        {
            UniquePtr<MakeParentCommand> command = CJING_MAKE_UNIQUE<MakeParentCommand>(*this, parent, child);
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

        EntityFolder* GetEntityFolder()override
        {
            if (editingScene != nullptr)
            {
                auto it = entityFolderMap.find(editingScene->GetGUID());
                if (it.isValid())
                    return it.value();
            }
            return nullptr;
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