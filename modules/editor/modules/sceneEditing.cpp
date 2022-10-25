#include "sceneEditing.h"
#include "level.h"
#include "editor\editor.h"
#include "editor\states\editorStateMachine.h"
#include "imgui-docking\imgui.h"
#include "level\level.h"

namespace VulkanTest
{
namespace Editor
{
	namespace
	{
		struct AddEmptyEntityCommand final : public IEditorCommand
		{
			AddEmptyEntityCommand(SceneEditingModule& worldEditor_, ECS::Entity* output_) :
				sceneEditing(worldEditor_),
				entity(ECS::INVALID_ENTITY),
				output(output_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity == ECS::INVALID_ENTITY);
				entity = sceneEditing.GetEditingWorld()->CreateEntity(nullptr);

				if (output)
					*output = entity;
				return true;
			}

			void Undo() override
			{
				ASSERT(entity);
				sceneEditing.GetEditingWorld()->DeleteEntity(entity);
			}

			const char* GetType() override {
				return "AddEmtpyEntity";
			}

		private:
			SceneEditingModule& sceneEditing;
			ECS::Entity entity;
			ECS::Entity* output;
		};

		struct DeleteEntityCommand final : public IEditorCommand
		{
			DeleteEntityCommand(SceneEditingModule& worldEditor_, ECS::Entity entity_) :
				sceneEditing(worldEditor_),
				entity(entity_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity != ECS::INVALID_ENTITY);
				sceneEditing.GetEditingWorld()->DeleteEntity(entity);
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
			SceneEditingModule& sceneEditing;
			ECS::Entity entity;
		};

		struct AddComponentCommand final : public IEditorCommand
		{
			AddComponentCommand(SceneEditingModule& worldEditor_, ECS::Entity entity_, ComponentType compType_) :
				sceneEditing(worldEditor_),
				entity(entity_),
				compType(compType_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity != ECS::INVALID_ENTITY);
				ASSERT(compType != INVALID_COMPONENT_TYPE);

				RenderScene* scene = dynamic_cast<RenderScene*>(sceneEditing.GetEditingWorld()->GetScene("Renderer"));
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
			SceneEditingModule& sceneEditing;
			ECS::Entity entity;
			ComponentType compType;
		};

		struct MakeParentCommand final : public IEditorCommand
		{
			MakeParentCommand(SceneEditingModule& worldEditor_, ECS::Entity parent_, ECS::Entity child_) :
				sceneEditing(worldEditor_),
				parent(parent_),
				child(child_)
			{
				oldFolder = sceneEditing.GetEditingScene()->GetFolders().GetFolder(child);
				oldParent = child_.GetParent();
			}

			bool Execute() override
			{
				ASSERT(child != ECS::INVALID_ENTITY);
				child.ChildOf(parent);
				if (parent != ECS::INVALID_ENTITY)
				{
					auto& folders = sceneEditing.GetEditingScene()->GetFolders();
					auto folder = folders.GetFolder(parent);
					folders.MoveToFolder(child, folder);
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
			SceneEditingModule& sceneEditing;
			ECS::Entity parent;
			ECS::Entity child;
			EntityFolder::FolderID oldFolder;
			ECS::Entity oldParent;
		};
	}

	struct SceneEditingModuleImpl : SceneEditingModule
	{
	private:
		EditorApp& app;
		Scene* editingScene = nullptr;
		Scene* nextEditScene = nullptr;
		Array<ECS::Entity> selectedEntities;
		WorldView* worldView = nullptr;

	public:
		SceneEditingModuleImpl(EditorApp& app_) :
			SceneEditingModule(app_),
			app(app_)
		{
		}

		void EndFrame()
		{
			if (nextEditScene != nullptr)
			{
				SetEditingScene(nextEditScene);
				nextEditScene = nullptr;
			}
		}

		void SetEditingScene(Scene* scene)
		{
			if (scene == editingScene)
				return;

			editingScene = scene;

			editor.OnSceneEditing(scene);
		}
	
		void SaveEditingScene() override
		{
			if (editingScene == nullptr)
				return;

			app.GetLevelModule().SaveScene(editingScene);
		}

		void EditScene(Scene* scene) override
		{
			if (scene == editingScene)
				return;

			nextEditScene = scene;
		}

		void CloseEditingScene() override
		{
			SetEditingScene(nullptr);
		}

		Scene* GetEditingScene() override
		{
			return editingScene;
		}

		World* GetEditingWorld() override
		{
			auto editingScene = GetEditingScene();
			return editingScene ? editingScene->GetWorld() : nullptr;
		}

		bool CheckSaveBeforeClose()
		{
			return true;
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

		const Array<ECS::Entity>& GetSelectedEntities() const override
		{
			return selectedEntities;
		}

		WorldView& GetView() override
		{
			return *worldView;
		}

		void SetView(WorldView& view_) override
		{
			worldView = &view_;
		}
	};

	SceneEditingModule::SceneEditingModule(EditorApp& app) :
		EditorModule(app)
	{
	}

	SceneEditingModule::~SceneEditingModule()
	{
	}

	SceneEditingModule* SceneEditingModule::Create(EditorApp& app)
	{
		return CJING_NEW(SceneEditingModuleImpl)(app);
	}
}
}
