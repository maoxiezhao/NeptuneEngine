#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\sceneView.h"
#include "renderer\model.h"

namespace VulkanTest
{
namespace Editor
{
	struct ModelPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;

	public:
		ModelPlugin(EditorApp& app_) : app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("obj", Model::ResType);
		}

		bool Compile(const Path& path)
		{
			return app.GetAssetCompiler().CopyCompile(path);
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "obj" };
		}
	};

	struct RenderPlugin : EditorPlugin
	{
	private:
		EditorApp& app;
		SceneView sceneView;

		ModelPlugin modelPlugin;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			modelPlugin(app_),
			sceneView(app_)
		{
		}

		virtual ~RenderPlugin() 
		{
			app.RemoveWidget(sceneView);
		}

		void Initialize() override
		{
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(modelPlugin);

			app.AddWidget(sceneView);

			sceneView.Init();
		}

		const char* GetName()const override
		{
			return "renderer";
		}
	};

	EditorPlugin* SetupPluginRenderer(EditorApp& app)
	{
		return CJING_NEW(RenderPlugin)(app);
	}
}
}
