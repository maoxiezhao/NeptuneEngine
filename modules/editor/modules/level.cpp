#include "level.h"
#include "sceneEditing.h"
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
	struct LevelModuleImpl : LevelModule
	{
	private:
		EditorApp& app;
		ScenePlugin scenePlugin;
		Array<Scene*> loadedScenes;

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

		void SaveAllScenes() override
		{
			for (auto scene : loadedScenes)
				SaveScene(scene);
		}

		Array<Scene*>& GetLoadedScenes() override
		{
			return loadedScenes;
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
					app.GetSceneEditingModule().EditScene(scene);
					return;
				}
			}

			loadedScenes.push_back(scene);
			app.GetSceneEditingModule().EditScene(scene);
		}

		void OnSceneUnloaded(Scene* scene, const Guid& sceneID)
		{
			ASSERT(scene);
			loadedScenes.erase(scene);
			
			auto& sceneEditing = app.GetSceneEditingModule();
			auto editingScene = sceneEditing.GetEditingScene();
			if (editingScene == scene && !loadedScenes.empty())
				sceneEditing.EditScene(loadedScenes.back());
			else
				sceneEditing.CloseEditingScene();
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
