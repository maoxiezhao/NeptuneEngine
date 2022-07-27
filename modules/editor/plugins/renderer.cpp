#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\sceneView.h"
#include "renderer\model.h"
#include "renderer\material.h"

#include "model\objImporter.h"
#include "shader\shaderCompilation.h"

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

	// Shader editor plugin
	struct ShaderPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;

		struct Meta
		{
			Array<Path> dependencies;
		};

	public:
		ShaderPlugin(EditorApp& app_) :
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("shd", Shader::ResType);
		}

		bool Compile(const Path& path)override
		{
			char ext[5] = {};
			CopyString(Span(ext), Path::GetExtension(path.ToSpan()));
			MakeLowercase(Span(ext), ext);

			if (EqualString(ext, "shd"))
			{
				OutputMemoryStream outMem;
				outMem.Reserve(32 * 1024);

				CompilationOptions options = {};
				options.Macros.clear();
				options.path = path;
				options.outMem = &outMem;
				if (!ShaderCompilation::Compile(app, options))
				{
					Logger::Error("Failed to import %s", path.c_str());
					return false;
				}

				// Update meta
				String metaData;
				metaData += "dependencies = {";
				HashMap<U64, bool> set;
				for (const auto& dependency : options.dependencies)
				{
					auto hash = RuntimeHash(dependency.c_str()).GetHashValue();
					if (set.find(hash).isValid())
						continue;
					set.insert(hash, true);

					metaData += "\"";
					metaData += dependency.c_str();
					metaData += "\",";
				}
				metaData += "\n}";
				app.GetAssetCompiler().UpdateMeta(path, metaData.c_str());

				return app.GetAssetCompiler().WriteCompiled(path.c_str(), Span(outMem.Data(), outMem.Size()));
			}
			else
			{
				ASSERT(false);
				return false;
			}
		}

		void RegisterResource(AssetCompiler& compiler, const char* path) override
		{
			compiler.AddResource(Shader::ResType, path);

			Meta meta = GetMeta(Path(path));
			for (const auto& dependency : meta.dependencies)
				compiler.AddDependency(Path(path), dependency);
		}

		Meta GetMeta(const Path& path)const
		{
			// TODO: Remove lua functions
			Meta meta = {};
			app.GetAssetCompiler().GetMeta(path, [&](lua_State* L) {
				lua_getglobal(L, "dependencies");
				if (lua_type(L, -1) == LUA_TTABLE)
				{
					lua_len(L, -1);
					auto count = lua_tointeger(L, -1);
					lua_pop(L, 1);

					for (int i = 0; i < count; ++i)
					{
						lua_rawgeti(L, -1, i + 1);
						if (lua_type(L, -1) == LUA_TSTRING)
							meta.dependencies.push_back(LuaUtils::Get<Path>(L, -1));
						lua_pop(L, 1);
					}
				}
				lua_pop(L, 1);
			});
			return meta;
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "shd" };
		}
	};

	struct RenderPlugin : EditorPlugin
	{
	private:
		EditorApp& app;
		SceneView sceneView;

		ModelPlugin modelPlugin;
		MaterialPlugin materialPlugin;
		ShaderPlugin shaderPlugin;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			modelPlugin(app_),
			materialPlugin(app_),
			shaderPlugin(app_),
			sceneView(app_)
		{
		}

		virtual ~RenderPlugin() 
		{
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.RemovePlugin(modelPlugin);
			assetCompiler.RemovePlugin(materialPlugin);
			assetCompiler.RemovePlugin(shaderPlugin);

			app.RemoveWidget(sceneView);
		}

		void Initialize() override
		{
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(modelPlugin);
			assetCompiler.AddPlugin(materialPlugin);
			assetCompiler.AddPlugin(shaderPlugin);

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
