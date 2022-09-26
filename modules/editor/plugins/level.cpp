#include "level.h"
#include "importers\resourceCreator.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\worldEditor.h"
#include "editor\states\editorStateMachine.h"
#include "editor\importers\resourceImportingManager.h"
#include "core\serialization\json.h"

#include "imgui-docking\imgui.h"
#include "level\level.h"

namespace VulkanTest
{
namespace Editor
{
	struct ScenePlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		ScenePlugin(EditorApp& app_) :
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("scene", SceneResource::ResType);

			Level::SceneLoaded.Bind<&ScenePlugin::OnSceneLoaded>(this);
			Level::SceneUnloaded.Bind<&ScenePlugin::OnSceneUnloaded>(this);
		}

		~ScenePlugin()
		{
			Level::SceneLoaded.Unbind<&ScenePlugin::OnSceneLoaded>(this);
			Level::SceneUnloaded.Unbind<&ScenePlugin::OnSceneUnloaded>(this);
		}

		bool Compile(const Path& path, Guid guid)override
		{
			FileSystem& fs = app.GetEngine().GetFileSystem();
			OutputMemoryStream mem;
			if (!fs.LoadContext(path.c_str(), mem))
			{
				Logger::Error("failed to read file:%s", path.c_str());
				return false;
			}

			return ResourceImportingManager::Create(app, [&](CreateResourceContext& ctx)->CreateResult {
				IMPORT_SETUP(SceneResource);
				DataChunk* shaderChunk = ctx.AllocateChunk(0);
				shaderChunk->mem.Link(mem.Data(), mem.Size());

				return CreateResult::Ok;
			}, guid, path);
		}

		std::vector<const char*> GetSupportExtensions() {
			return { "scene" };
		}

		void OnGui(Span<class Resource*> resource)override {}

		ResourceType GetResourceType() const override {
			return SceneResource::ResType;
		}

		bool CreateResource(const Path& path, const char* name) 
		{
			bool ret = true;
			Engine& engine = app.GetEngine();
			auto& fs = engine.GetFileSystem();
			auto scene = CJING_NEW(Scene)();
			scene->SetName(name);

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
				auto file = fs.OpenFile(fullPath, FileFlags::DEFAULT_WRITE);
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

		void DoubleClick(const Path& path) override
		{
			const ResourceType resType = app.GetAssetCompiler().GetResourceType(path.c_str());
			if (resType == SceneResource::ResType)
				OpenScene(path);
		}

		bool CreateResourceEnable()const {
			return true;
		}

		const char* GetResourceName() const {
			return "Scene";
		}

	public:
		void OpenScene(const Path& path)
		{
			if (!app.GetStateMachine().CurrentState()->CanChangeScene())
				return;

			auto resource = ResourceManager::LoadResource<SceneResource>(path);
			if (!resource)
				return;

			app.GetStateMachine().GetChangingScenesState()->LoadScene(resource->GetGUID());
		}

		void SaveScene(Scene* scene)
		{
			Level::SaveSceneAsync(scene);
		}

		void CloseScene(Scene* scene)
		{
			if (!app.GetStateMachine().CurrentState()->CanChangeScene())
				return;

			if (!CheckSaveBeforeClose())
				return;

			app.GetStateMachine().GetChangingScenesState()->UnloadScene(scene);
		}

		bool CheckSaveBeforeClose()
		{
			return true;
		}

		void OnSceneLoaded(Scene* scene, const Guid& sceneID)
		{
			app.GetWorldEditor().OpenScene(scene);
		}

		void OnSceneUnloaded(Scene* scene, const Guid& sceneID)
		{
			app.GetWorldEditor().CloseScene(scene);
		}
	};

	struct LevelPluginImpl : LevelPlugin
	{
	private:
		EditorApp& app;
		ScenePlugin scenePlugin;

	public:
		LevelPluginImpl(EditorApp& app_) :
			app(app_),
			scenePlugin(app_)
		{
		}

		virtual ~LevelPluginImpl()
		{
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.RemovePlugin(scenePlugin);
		
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.RemovePlugin(scenePlugin);
		}

		void Initialize() override
		{
			// Add plugins for asset compiler
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(scenePlugin);

			// Add plugins for asset browser
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.AddPlugin(scenePlugin);
		}

		void SaveScene(Scene* scene) override
		{
			scenePlugin.SaveScene(scene);
		}

		void CloseScene(Scene* scene)override
		{
			scenePlugin.CloseScene(scene);
		}

		const char* GetName()const override
		{
			return "level";
		}
	};

	EditorPlugin* SetupPluginLevel(EditorApp& app)
	{
		return CJING_NEW(LevelPluginImpl)(app);
	}
}
}
