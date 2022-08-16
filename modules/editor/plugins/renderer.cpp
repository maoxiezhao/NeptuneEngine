#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\sceneView.h"
#include "renderer\model.h"
#include "renderer\materials\material.h"
#include "gpu\vulkan\typeToString.h"
#include "imgui-docking\imgui.h"

#include "editor\plugins\model\objImporter.h"
#include "editor\plugins\shader\shaderCompilation.h"
#include "editor\plugins\texture\textureImporter.h"
#include "editor\plugins\material\materialImporter.h"

namespace VulkanTest
{
namespace Editor
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Texture editor plugin
	struct TexturePlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;
		Texture* curTexture = nullptr;

		struct TextureMeta
		{
			bool generateMipmaps = true;
			bool compress = false;
		};

	public:
		TexturePlugin(EditorApp& app_) :
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("raw", Texture::ResType);
			app_.GetAssetCompiler().RegisterExtension("png", Texture::ResType);
			app_.GetAssetCompiler().RegisterExtension("jpg", Texture::ResType);
			app_.GetAssetCompiler().RegisterExtension("tga", Texture::ResType);
		}

		bool Compile(const Path& path)override
		{
			TextureMeta meta = GetMeta(path);
			TextureImporter::ImportConfig config;
			config.compress = meta.compress;
			config.generateMipmaps = meta.generateMipmaps;
			if (!TextureImporter::Import(app, path.c_str(), config))
				return false;

			return true;
		}

		TextureMeta GetMeta(const Path& path)const
		{
			TextureMeta meta = {};
			return meta;
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "raw", "png", "jpg", "tga" };
		}

		void OnGui(Span<class Resource*> resource)override
		{
			if (resource.length() > 1)
				return;

			Shader* shader = static_cast<Shader*>(resource[0]);
			if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
				app.GetAssetBrowser().OpenInExternalEditor(shader->GetPath().c_str());

			auto* texture = static_cast<Texture*>(resource[0]);
			const auto& imgInfo = texture->GetImageInfo();

			ImGuiEx::Label("Size");
			ImGui::Text("%dx%d", imgInfo.width, imgInfo.height);
			ImGuiEx::Label("Mips");
			ImGui::Text("%d", imgInfo.levels);
			ImGuiEx::Label("Format");
			ImGui::TextUnformatted(GPU::FormatToString(imgInfo.format));

			if (texture->GetImage())
			{
				ImVec2 textureSize(200, 200);
				if (imgInfo.width > imgInfo.height)
					textureSize.y = textureSize.x * imgInfo.height / imgInfo.width;
				else
					textureSize.x = textureSize.y * imgInfo.width / imgInfo.height;

				ImGui::Image(texture->GetImage(), textureSize);
			}
		}

		ResourceType GetResourceType() const override {
			return Texture::ResType;
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Material editor plugin
	struct MaterialPlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
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
			return MaterialImporter::Import(app, path);
		}

		void OnGui(Span<class Resource*> resource)override
		{
			if (resource.length() > 1)
				return;

			Material* material = static_cast<Material*>(resource[0]);
			if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
				app.GetAssetBrowser().OpenInExternalEditor(material->GetPath().c_str());

		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "mat" };
		}

		ResourceType GetResourceType() const override {
			return Material::ResType;
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////
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
					return false;

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

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Shader editor plugin
	struct ShaderPlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
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
					return false;

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

		void OnGui(Span<class Resource*> resource)override
		{
			if (resource.length() > 1)
				return;

			Shader* shader = static_cast<Shader*>(resource[0]);
			if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
				app.GetAssetBrowser().OpenInExternalEditor(shader->GetPath().c_str());
		}

		std::vector<const char*> GetSupportExtensions() {
			return { "shd" };
		}

		ResourceType GetResourceType() const override {
			return Shader::ResType;
		}
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	struct RenderInterfaceImpl final : RenderInterface
	{
	private:
		EditorApp& app;
		HashMap<void*, ResPtr<Texture>> textures;

	public:
		RenderInterfaceImpl(EditorApp& app_) : app(app_) {}

		virtual void* LoadTexture(const Path& path) override
		{
			auto& resManager = app.GetEngine().GetResourceManager();
			ResPtr<Texture> texture = resManager.LoadResourcePtr<Texture>(path);
			textures.insert(texture->GetImage(), texture);
			return texture->GetImage();
		}
		
		virtual void* CreateTexture(const char* name, const void* pixels, int w, int h) override
		{
			auto& resManager = app.GetEngine().GetResourceManager();
			Texture* texture = CJING_NEW(Texture)(Path(name), *resManager.GetFactory(Texture::ResType));
			if (!texture->Create(w, h, VK_FORMAT_R8G8B8A8_UNORM, pixels))
			{
				Logger::Error("Failed to create the texture.");
				CJING_SAFE_DELETE(texture);
				return nullptr;
			}
			texture->SetOwnedBySelf(true);
			textures.insert(texture->GetImage(), ResPtr<Texture>(texture));
			return texture->GetImage();
		}
		
		virtual void DestroyTexture(void* handle) override
		{
			auto it = textures.find(handle);
			if (!it.isValid())
				return;

			auto texture = it.value();
			textures.erase(it);
			texture->Destroy();
		}
	};

	struct RenderPlugin : EditorPlugin
	{
	private:
		EditorApp& app;
		SceneView sceneView;
		RenderInterfaceImpl renderInterface;

		TexturePlugin texturePlugin;
		ModelPlugin modelPlugin;
		MaterialPlugin materialPlugin;
		ShaderPlugin shaderPlugin;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			renderInterface(app_),
			texturePlugin(app_),
			modelPlugin(app_),
			materialPlugin(app_),
			shaderPlugin(app_),
			sceneView(app_)
		{
		}

		virtual ~RenderPlugin() 
		{
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.RemovePlugin(shaderPlugin);
			assetBrowser.RemovePlugin(texturePlugin);
			assetBrowser.RemovePlugin(materialPlugin);

			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.RemovePlugin(modelPlugin);
			assetCompiler.RemovePlugin(materialPlugin);
			assetCompiler.RemovePlugin(shaderPlugin);
			assetCompiler.RemovePlugin(texturePlugin);

			app.RemoveWidget(sceneView);
			app.SetRenderInterace(nullptr);
		}

		void Initialize() override
		{
			// Set render interface
			app.SetRenderInterace(&renderInterface);

			// Add plugins for asset compiler
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(texturePlugin);
			assetCompiler.AddPlugin(modelPlugin);
			assetCompiler.AddPlugin(materialPlugin);
			assetCompiler.AddPlugin(shaderPlugin);

			// Add plugins for asset browser
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.AddPlugin(texturePlugin);
			assetBrowser.AddPlugin(shaderPlugin);
			assetBrowser.AddPlugin(materialPlugin);

			// Add widget for editor
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
