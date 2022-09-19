#include "level.h"
#include "importers\resourceCreator.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"
#include "level\sceneResource.h"
#include "editor\importers\resourceImportingManager.h"

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

		bool CreateResource(const Path& path, const char* name) {
			Logger::Info("Create new scene %s", name);
			return true;
		}

		bool CreateResourceEnable()const {
			return true;
		}

		const char* GetResourceName() const {
			return "Scene";
		}
	};

	struct LevelPlugin : EditorPlugin
	{
	private:
		EditorApp& app;

		ScenePlugin scenePlugin;

	public:
		LevelPlugin(EditorApp& app_) :
			app(app_),
			scenePlugin(app_)
		{
		}

		virtual ~LevelPlugin()
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

		bool ShowComponentGizmo(WorldView& worldView, ECS::Entity entity, ECS::EntityID compID) override
		{
			return false;
		}

		const char* GetName()const override
		{
			return "scene";
		}
	};

	EditorPlugin* SetupPluginLevel(EditorApp& app)
	{
		return CJING_NEW(LevelPlugin)(app);
	}
}
}
