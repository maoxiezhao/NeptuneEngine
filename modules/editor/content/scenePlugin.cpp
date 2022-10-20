#include "scenePlugin.h"
#include "editor\states\editorStateMachine.h"
#include "editor\editor.h"
#include "editor\widgets\worldEditor.h"
#include "editor\widgets\propertyEditor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	ScenePlugin::ScenePlugin(EditorApp& app_) :
		app(app_)
	{
		Level::SceneLoaded.Bind<&ScenePlugin::OnSceneLoaded>(this);
		Level::SceneUnloaded.Bind<&ScenePlugin::OnSceneUnloaded>(this);
	}

	ScenePlugin::~ScenePlugin()
	{
		Level::SceneLoaded.Unbind<&ScenePlugin::OnSceneLoaded>(this);
		Level::SceneUnloaded.Unbind<&ScenePlugin::OnSceneUnloaded>(this);
	}

	void ScenePlugin::OnGui(Span<class Resource*> resource)
	{
		if (resource.length() > 1)
			return;
	}

	bool ScenePlugin::CreateResource(const Path& path, const char* name)
	{
		bool ret = true;
		Engine& engine = app.GetEngine();
		auto scene = CJING_NEW(Scene)();
		scene->SetName(name);

		// Serialize scene datas
		rapidjson_flax::StringBuffer outData;
		if (!Level::SaveScene(scene, outData))
		{
			Logger::Warning("Failed to save scene %s/%s", path.c_str(), name);
			ret = false;
			goto ResFini;
		}

		// Write scene file
		{
			StaticString<MAX_PATH_LENGTH> fullPath(path.c_str(), "/", name, ".scene");
			auto file = FileSystem::OpenFile(fullPath, FileFlags::DEFAULT_WRITE);
			if (!file->IsValid())
			{
				Logger::Error("Failed to create the scene resource file");
				ret = false;
				goto ResFini;
			}

			file->Write(outData.GetString(), outData.GetSize());
			file->Close();

			// Register scene resource
			ResourceManager::GetCache().Register(scene->GetGUID(), SceneResource::ResType, Path(fullPath));
		}

	ResFini:
		CJING_SAFE_DELETE(scene);
		return ret;
	}

	void ScenePlugin::DoubleClick(const Path& path)
	{
	}

	void ScenePlugin::OnSceneLoaded(Scene* scene, const Guid& sceneID)
	{
		app.GetWorldEditor().OpenScene(scene);
	}

	void ScenePlugin::OnSceneUnloaded(Scene* scene, const Guid& sceneID)
	{
		app.GetWorldEditor().CloseScene(scene);
	}

	void ScenePlugin::OpenScene(const Path& path)
	{
		if (!app.GetStateMachine().CurrentState()->CanChangeScene())
			return;

		auto resource = ResourceManager::LoadResource<SceneResource>(path);
		if (!resource)
			return;

		app.GetStateMachine().GetChangingScenesState()->LoadScene(resource->GetGUID());
	}

	void ScenePlugin::SaveScene(Scene* scene)
	{
		Level::SaveSceneAsync(scene);
	}

	void ScenePlugin::CloseScene(Scene* scene)
	{
		if (!app.GetStateMachine().CurrentState()->CanChangeScene())
			return;

		if (!CheckSaveBeforeClose())
			return;

		app.GetStateMachine().GetChangingScenesState()->UnloadScene(scene);
	}
}
}