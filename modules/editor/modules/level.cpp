#include "level.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\sceneView.h"
#include "editor\content\scenePlugin.h"
#include "editor\states\editorStateMachine.h"
#include "contentImporters\resourceImportingManager.h"
#include "core\serialization\json.h"

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
			AddEmptyEntityCommand(LevelModule& worldEditor_, ECS::Entity* output_) :
				levelModule(worldEditor_),
				entity(ECS::INVALID_ENTITY),
				output(output_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity == ECS::INVALID_ENTITY);
				entity = levelModule.GetEditingWorld()->CreateEntity(nullptr);

				if (output)
					*output = entity;
				return true;
			}

			void Undo() override
			{
				ASSERT(entity);
				levelModule.GetEditingWorld()->DeleteEntity(entity);
			}

			const char* GetType() override {
				return "AddEmtpyEntity";
			}

		private:
			LevelModule& levelModule;
			ECS::Entity entity;
			ECS::Entity* output;
		};

		struct DeleteEntityCommand final : public IEditorCommand
		{
			DeleteEntityCommand(LevelModule& worldEditor_, ECS::Entity entity_) :
				levelModule(worldEditor_),
				entity(entity_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity != ECS::INVALID_ENTITY);
				levelModule.GetEditingWorld()->DeleteEntity(entity);
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
			LevelModule& levelModule;
			ECS::Entity entity;
		};

		struct AddComponentCommand final : public IEditorCommand
		{
			AddComponentCommand(LevelModule& worldEditor_, ECS::Entity entity_, ComponentType compType_) :
				levelModule(worldEditor_),
				entity(entity_),
				compType(compType_)
			{
			}

			bool Execute() override
			{
				ASSERT(entity != ECS::INVALID_ENTITY);
				ASSERT(compType != INVALID_COMPONENT_TYPE);

				RenderScene* scene = dynamic_cast<RenderScene*>(levelModule.GetEditingWorld()->GetScene("Renderer"));
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
			LevelModule& levelModule;
			ECS::Entity entity;
			ComponentType compType;
		};

		struct MakeParentCommand final : public IEditorCommand
		{
			MakeParentCommand(LevelModule& worldEditor_, ECS::Entity parent_, ECS::Entity child_) :
				levelModule(worldEditor_),
				parent(parent_),
				child(child_)
			{
				oldFolder = levelModule.GetEditingScene()->GetFolders().GetFolder(child);
				oldParent = child_.GetParent();
			}

			bool Execute() override
			{
				ASSERT(child != ECS::INVALID_ENTITY);
				child.ChildOf(parent);
				if (parent != ECS::INVALID_ENTITY)
				{
					auto& folders = levelModule.GetEditingScene()->GetFolders();
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
			LevelModule& levelModule;
			ECS::Entity parent;
			ECS::Entity child;
			EntityFolder::FolderID oldFolder;
			ECS::Entity oldParent;
		};
	}

	struct LevelModuleImpl : LevelModule
	{
	private:
		EditorApp& app;
		ScenePlugin scenePlugin;
		Array<Scene*> loadedScenes;
		Scene* editingScene = nullptr;
		Scene* nextEditScene = nullptr;
		Array<ECS::Entity> selectedEntities;
		WorldView* worldView = nullptr;

	public:
		LevelModuleImpl(EditorApp& app_) :
			LevelModule(app_),
			app(app_),
			scenePlugin(app_)
		{
		}

		void Initialize() override
		{
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.AddPlugin(scenePlugin);

			Level::SceneLoaded.Bind<&LevelModuleImpl::OnSceneLoaded>(this);
			Level::SceneUnloaded.Bind<&LevelModuleImpl::OnSceneUnloaded>(this);
		}

		void Unintialize() override
		{
			Level::SceneLoaded.Unbind<&LevelModuleImpl::OnSceneLoaded>(this);
			Level::SceneUnloaded.Unbind<&LevelModuleImpl::OnSceneUnloaded>(this);

			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.RemovePlugin(scenePlugin);
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

		void OpenScene(const Path& path) override
		{
			if (!app.GetStateMachine().CurrentState()->CanChangeScene())
				return;

			if (!CheckSaveBeforeClose())
				return;

			auto resource = ResourceManager::LoadResource<SceneResource>(path);
			if (!resource)
				return;

			app.GetStateMachine().GetChangingScenesState()->LoadScene(resource->GetGUID());
		}

		void OpenScene(const Guid& guid) override
		{
			if (!app.GetStateMachine().CurrentState()->CanChangeScene())
				return;

			if (!CheckSaveBeforeClose())
				return;

			app.GetStateMachine().GetChangingScenesState()->LoadScene(guid);
		}

		void SaveScene(Scene* scene) override
		{
			Level::SaveSceneAsync(scene);
		}

		void CloseScene(Scene* scene)override
		{
			if (!app.GetStateMachine().CurrentState()->CanChangeScene())
				return;

			if (!CheckSaveBeforeClose())
				return;

			app.GetStateMachine().GetChangingScenesState()->UnloadScene(scene);
		}

		void SaveEditingScene() override
		{
			if (editingScene == nullptr)
				return;

			SaveScene(editingScene);
		}

		void SaveAllScenes() override
		{
			for (auto scene : loadedScenes)
				SaveScene(scene);
		}

		Array<Scene*>& GetLoadedScenes() override
		{
			return loadedScenes;
		}

		void EditScene(Scene* scene) override
		{
			if (scene == editingScene)
				return;

			nextEditScene = scene;
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

		void OnSceneLoaded(Scene* scene, const Guid& sceneID)
		{
			ASSERT(scene);
			for (auto scene : loadedScenes)
			{
				if (scene == scene)
				{
					EditScene(scene);
					return;
				}
			}

			loadedScenes.push_back(scene);
			EditScene(scene);
		}

		void OnSceneUnloaded(Scene* scene, const Guid& sceneID)
		{
			ASSERT(scene);
			loadedScenes.erase(scene);
			
			if (editingScene == scene && !loadedScenes.empty())
				EditScene(loadedScenes.back());
			else
				SetEditingScene(nullptr);
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

	LevelModule::LevelModule(EditorApp& app) :
		EditorModule(app)
	{
	}

	LevelModule::~LevelModule()
	{
	}

	LevelModule* LevelModule::Create(EditorApp& app)
	{
		return CJING_NEW(LevelModuleImpl)(app);
	}
}
}
