#include "level.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\worldEditor.h"
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
		}

		void Unintialize() override
		{
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.RemovePlugin(scenePlugin);
		}

		void SaveScene(Scene* scene) override
		{
			scenePlugin.SaveScene(scene);
		}

		void CloseScene(Scene* scene)override
		{
			scenePlugin.CloseScene(scene);
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
