#include "renderer.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"
#include "editor\widgets\assetImporter.h"
#include "editor\widgets\sceneView.h"
#include "content\resources\model.h"
#include "renderer\render2D\fontResource.h"
#include "core\scene\reflection.h"
#include "gpu\vulkan\typeToString.h"
#include "math\vMath_impl.hpp"
#include "renderer\debugDraw.h"
#include "imgui-docking\imgui.h"

#include "editor\content\fontPlugin.h"
#include "editor\content\materialPlugin.h"
#include "editor\content\modelPlugin.h"
#include "editor\content\texturePlugin.h"
#include "editor\content\shaderPlugin.h"

namespace VulkanTest
{
namespace Editor
{
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

			ResPtr<Texture> texture = ResourceManager::LoadResourceInternal<Texture>(path);
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

			app.RemoveWidget(sceneView);
			app.SetRenderInterace(nullptr);
		}

		void Initialize() override
		{
			// Set render interface
			app.SetRenderInterace(&renderInterface);

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
