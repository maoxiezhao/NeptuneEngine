#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\sceneView.h"
#include "renderer\model.h"
#include "renderer\material.h"
#include "objImporter.h"

namespace VulkanTest
{
namespace Editor
{
	// Material editor plugin
	struct MaterialPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;

	public:
		MaterialPlugin(EditorApp& app_) :
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("mat", Material::ResType);
		}
	
		bool Compile(const Path& path)override
		{
			return app.GetAssetCompiler().CopyCompile(path);
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "mat" };
		}
	};

	// Model editor plugin
	struct ModelPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;
		OBJImporter objImporter;

		struct Meta
		{
			F32 scale = 1.0f;
		};

	public:
		ModelPlugin(EditorApp& app_) : 
			app(app_),
			objImporter(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("obj", Model::ResType);
		}

		bool Compile(const Path& path)override
		{
			char ext[5] = {};
			CopyString(Span(ext), Path::GetExtension(path.ToSpan()));
			MakeLowercase(Span(ext), ext);

			if (EqualString(ext, "obj"))
			{
				Meta meta = GetMeta(path);
				OBJImporter::ImportConfig cfg = {};
				cfg.scale = meta.scale;

				if (!objImporter.Import(path.c_str()))
				{
					Logger::Error("Failed to import %s", path.c_str());
					return false;
				}

				objImporter.WriteModel(path.c_str(), cfg);
				objImporter.WriteMaterials(path.c_str(), cfg);
				return true;
			}
			else
			{
				ASSERT(false);
				return false;
			}
		}

		Meta GetMeta(const Path& path)const
		{
			Meta meta = {};

			return meta;
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
		MaterialPlugin materialPlugin;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			modelPlugin(app_),
			materialPlugin(app_),
			sceneView(app_)
		{
		}

		virtual ~RenderPlugin() 
		{
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.RemovePlugin(modelPlugin);
			assetCompiler.RemovePlugin(materialPlugin);

			app.RemoveWidget(sceneView);
		}

		void Initialize() override
		{
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(modelPlugin);
			assetCompiler.AddPlugin(materialPlugin);

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
