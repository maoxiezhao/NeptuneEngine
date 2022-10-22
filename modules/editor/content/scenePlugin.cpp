#include "scenePlugin.h"
#include "editor\states\editorStateMachine.h"
#include "editor\editor.h"
#include "editor\widgets\propertyEditor.h"
#include "editor\modules\level.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
	namespace Editor
	{
		ScenePlugin::ScenePlugin(EditorApp& app_) :
			app(app_)
		{

		}

		ScenePlugin::~ScenePlugin()
		{
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

		void ScenePlugin::Open(const AssetItem& item)
		{
			const SceneItem* sceneItem = dynamic_cast<const SceneItem*>(&item);
			if (sceneItem)
				app.GetLevelModule().OpenScene(sceneItem->id);
		}

		AssetItem* ScenePlugin::ConstructItem(const Path& path, const ResourceType& type, const Guid& guid)
		{
			return CJING_NEW(SceneItem)(path, guid);
		}
}
}