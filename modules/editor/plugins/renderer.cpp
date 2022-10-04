#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\widgets\sceneView.h"
#include "content\resources\model.h"
#include "renderer\render2D\fontResource.h"
#include "core\scene\reflection.h"
#include "gpu\vulkan\typeToString.h"
#include "math\vMath_impl.hpp"
#include "renderer\debugDraw.h"
#include "imgui-docking\imgui.h"

#include "editor\importers\resourceImportingManager.h"
#include "editor\importers\model\modelImporter.h"
#include "editor\importers\shader\shaderCompilation.h"
#include "editor\importers\texture\textureImporter.h"
#include "editor\importers\material\materialPlugin.h"

namespace VulkanTest
{
namespace Editor
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Font editor plugin
	struct FontPlugin final : AssetCompiler::IPlugin, AssetBrowser::IPlugin
	{
	private:
		EditorApp& app;

	public:
		FontPlugin(EditorApp& app_) :
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("ttf", FontResource::ResType);
		}

		bool Compile(const Path& path, Guid guid)override
		{
			OutputMemoryStream mem;
			if (!FileSystem::LoadContext(path.c_str(), mem))
			{
				Logger::Error("failed to read file:%s", path.c_str());
				return false;
			}

			const Path& resPath = ResourceStorage::GetContentPath(path, true);
			return ResourceImportingManager::Create([&](CreateResourceContext& ctx)->CreateResult {
				IMPORT_SETUP(FontResource);

				// Write header
				FontResource::FontHeader header = {};
				ctx.WriteCustomData(header);

				// Write data
				DataChunk* shaderChunk = ctx.AllocateChunk(0);
				shaderChunk->mem.Link(mem.Data(), mem.Size());

				return CreateResult::Ok;
			}, guid, path, resPath);
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { "ttf" };
		}

		void OnGui(Span<class Resource*> resource)override{}

		ResourceType GetResourceType() const override {
			return FontResource::ResType;
		}
	};

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

		bool Compile(const Path& path, Guid guid)override
		{
			TextureMeta meta = GetMeta(path);
			TextureImporter::ImportConfig config;
			config.compress = meta.compress;
			config.generateMipmaps = meta.generateMipmaps;

			const Path& resPath = ResourceStorage::GetContentPath(path, true);
			return ResourceImportingManager::Import(path, resPath, guid, &config);
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
	// Model editor plugin
	struct ModelPlugin final : AssetCompiler::IPlugin
	{
	private:
		EditorApp& app;

		struct Meta
		{
			F32 scale = 1.0f;
		};

	public:
		ModelPlugin(EditorApp& app_) : 
			app(app_)
		{
			app_.GetAssetCompiler().RegisterExtension("obj", Model::ResType);
			app_.GetAssetCompiler().RegisterExtension("gltf", Model::ResType);
		}

		bool Compile(const Path& path, Guid guid)override
		{			
			Meta meta = GetMeta(path);
			ModelImporter::ImportConfig cfg = {};
			cfg.scale = meta.scale;

			const Path& resPath = ResourceStorage::GetContentPath(path, true);
			return ResourceImportingManager::Import(path, resPath, guid, &cfg);
		}

		Meta GetMeta(const Path& path)const
		{
			Meta meta = {};
			return meta;
		}

		std::vector<const char*> GetSupportExtensions()
		{
			return { 
				"obj", 
				"gltf" 
			};
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

		bool Compile(const Path& path, Guid guid)override
		{
			char ext[5] = {};
			CopyString(Span(ext), Path::GetExtension(path.ToSpan()));
			MakeLowercase(Span(ext), ext);

			if (EqualString(ext, "shd"))
			{
				OutputMemoryStream outMem;
				outMem.Reserve(32 * 1024);

				OutputMemoryStream mem;
				CompilationOptions options = {};
				options.Macros.clear();
				options.path = path;
				options.outMem = &mem;
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

				return ShaderCompilation::Write(app, guid, path, mem);
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
		HashMap<U64, Texture*> pathMapping;

	public:
		RenderInterfaceImpl(EditorApp& app_) : app(app_) {}

		virtual void* LoadTexture(const Path& path) override
		{
			auto it = pathMapping.find(path.GetHashValue());
			if (it.isValid())
				return it.value()->GetImage();

			ResPtr<Texture> texture = ResourceManager::LoadResource<Texture>(path);
			texture->WaitForLoaded();
			textures.insert(texture->GetImage(), texture);
			pathMapping.insert(texture->GetPath().GetHashValue(), texture.get());
			return texture->GetImage();
		}
		
		virtual void* CreateTexture(const char* name, const void* pixels, int w, int h) override
		{
			Texture* texture = CJING_NEW(Texture)(ResourceInfo::Temporary(Texture::ResType));
			if (!texture->Create(w, h, VK_FORMAT_R8G8B8A8_UNORM, pixels))
			{
				Logger::Error("Failed to create the texture.");
				CJING_SAFE_DELETE(texture);
				return nullptr;
			}
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
			pathMapping.erase(texture->GetPath().GetHashValue());
			texture->Destroy();
		}
	};

	struct AddLightComponentPlugin final : IAddComponentPlugin
	{
	public:
		EditorApp& editor;

	public:
		explicit AddLightComponentPlugin(EditorApp& editor_) :
			editor(editor_)
		{}

		virtual void OnGUI(bool createEntity, bool fromFilter, WorldEditor& editor) override
		{
			ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
			auto CreateLight = [&]()->LightComponent* {
				if (createEntity)
				{
					ECS::Entity entity = editor.AddEmptyEntity();
					editor.SelectEntities(Span(&entity, 1), false);
				}
				const auto& selectedEntities = editor.GetSelectedEntities();
				if (selectedEntities.empty())
					return nullptr;

				auto compType = Reflection::GetComponentType("Light");
				editor.AddComponent(selectedEntities[0], compType);
				return selectedEntities[0].GetMut<LightComponent>();
			};

			if (ImGui::BeginMenu("Lights"))
			{
				if (ImGui::MenuItem("DirectionalLight"))
				{
					auto light = CreateLight();
					if (light != nullptr)
					{
						light->type = LightComponent::DIRECTIONAL;
						light->intensity = 10.0f;
					}
				}
				if (ImGui::MenuItem("PointLight"))
				{
					auto light = CreateLight();
					if (light != nullptr)
					{
						light->type = LightComponent::POINT;
						light->intensity = 20.0f;
					}
				}
				if (ImGui::MenuItem("SpotLight"))
				{
					auto light = CreateLight();
					if (light != nullptr)
					{
						light->type = LightComponent::SPOT;
						light->intensity = 50.0f;
					}
				}
				ImGui::EndMenu();
			}
		}

		const char* GetLabel() const override {
			return "Lights";
		}
	};

	struct RenderPlugin : EditorPlugin
	{
	private:
		EditorApp& app;
		SceneView sceneView;
		RenderInterfaceImpl renderInterface;

		FontPlugin fontPlugin;
		TexturePlugin texturePlugin;
		ModelPlugin modelPlugin;
		MaterialPlugin materialPlugin;
		ShaderPlugin shaderPlugin;

	public:
		RenderPlugin(EditorApp& app_) :
			app(app_),
			renderInterface(app_),
			fontPlugin(app_),
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
			assetBrowser.RemovePlugin(fontPlugin);

			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.RemovePlugin(modelPlugin);
			assetCompiler.RemovePlugin(materialPlugin);
			assetCompiler.RemovePlugin(shaderPlugin);
			assetCompiler.RemovePlugin(texturePlugin);
			assetCompiler.RemovePlugin(fontPlugin);

			app.RemoveWidget(sceneView);
			app.SetRenderInterace(nullptr);
		}

		void Initialize() override
		{
			// Set render interface
			app.SetRenderInterace(&renderInterface);

			// Add plugins for asset compiler
			AssetCompiler& assetCompiler = app.GetAssetCompiler();
			assetCompiler.AddPlugin(fontPlugin);
			assetCompiler.AddPlugin(texturePlugin);
			assetCompiler.AddPlugin(modelPlugin);
			assetCompiler.AddPlugin(materialPlugin);
			assetCompiler.AddPlugin(shaderPlugin);

			// Add plugins for asset browser
			AssetBrowser& assetBrowser = app.GetAssetBrowser();
			assetBrowser.AddPlugin(fontPlugin);
			assetBrowser.AddPlugin(texturePlugin);
			assetBrowser.AddPlugin(shaderPlugin);
			assetBrowser.AddPlugin(materialPlugin);

			// Add widget for editor
			app.AddWidget(sceneView);
			sceneView.Init();
		}

		bool ShowComponentGizmo(WorldView& worldView, ECS::Entity entity, ECS::EntityID compID) override
		{
			auto world = worldView.GetWorld();
			if (compID == world->GetComponentID<LightComponent>())
			{
				const LightComponent* light = entity.Get<LightComponent>();
				switch (light->type)
				{
				case LightComponent::DIRECTIONAL:
				{
					F32 dist = std::max(Distance(light->position, worldView.GetCamera().eye) * 0.5f, 0.0001f);
					F32x3 start = light->position;
					F32x3 end = light->position + light->direction * -dist;
					DebugDraw::DrawLine(start, end);
				}
				break;
				case LightComponent::POINT:
				{
					Sphere sphere;
					sphere.center = light->position;
					sphere.radius = light->range;
					DebugDraw::DrawSphere(sphere, F32x4(0.5f));
				}
				break;
				case LightComponent::SPOT:
				{
					F32x3 start = light->position;
					F32x3 end = light->position + light->direction * -light->range;

					auto rotMat = MatrixRotationQuaternion(LoadF32x4(light->rotation));
					const U32 segments = 16;
					const F32 range = light->range * std::sinf(light->outerConeAngle);
					for (I32 i = 0; i < segments; i++)
					{
						const F32 angleA = F32(i) / F32(segments) * MATH_2PI;
						const F32 angleB = F32(i + 1) / F32(segments) * MATH_2PI;

						F32x3 pointA = StoreF32x3(Vector3TransformNormal(LoadF32x3(F32x3(range * std::sinf(angleA), 0.0f, range * std::cosf(angleA))), rotMat)) + end;
						F32x3 pointB = StoreF32x3(Vector3TransformNormal(LoadF32x3(F32x3(range * std::sinf(angleB), 0.0f, range * std::cosf(angleB))), rotMat)) + end;
						DebugDraw::DrawLine(pointA, pointB);

						if (i % 4 == 0)
							DebugDraw::DrawLine(start, pointA);
					}
				}
				break;
				default:
					break;
				}
			}
			else if (compID == world->GetComponentID<ObjectComponent>())
			{
				const ObjectComponent* obj = entity.Get<ObjectComponent>();
				DebugDraw::DrawBox(StoreFMat4x4(obj->aabb.GetAsMatrix()), F32x4(0.5f));
			}
			return true;
		}

		void OnEditingSceneChanged(Scene* newScene, Scene* prevScene) override
		{
			if (newScene != nullptr)
			{
				AddLightComponentPlugin* addLightsPlugin = CJING_NEW(AddLightComponentPlugin)(app);
				auto compType = Reflection::GetComponentType("Light");
				app.RegisterComponent(ICON_FA_LIGHTBULB, compType, addLightsPlugin);
			}

			sceneView.OnEditingSceneChanged(newScene, prevScene);
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
