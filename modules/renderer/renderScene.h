#pragma once

#include "rendererCommon.h"
#include "renderGraph.h"
#include "shaderInterop_renderer.h"
#include "enums.h"
#include "models\model.h"

namespace VulkanTest
{
	struct RendererPlugin;

	struct TransformComponent
	{
		Transform transform;
	};

	struct CameraComponent
	{
		F32 fov = MATH_PI / 3.0f;
		F32 nearZ = 0.1f;
		F32 farZ = 1000.0f;
		F32 width = 0.0f;
		F32 height = 0.0f;

		F32x3 eye = F32x3(0.0f, 0.0f, 1.0f);
		F32x3 at = F32x3(0.0f, 0.0f, -1.0f);
		F32x3 up = F32x3(0.0f, 1.0f, 0.0f);
		FMat4x4 view;
		FMat4x4 projection;
		FMat4x4 viewProjection;
		FMat3x3 rotationMat;

		Frustum frustum;

		void UpdateCamera();
		void UpdateTransform(const Transform& transform);

		MATRIX GetViewProjection() const {
			return LoadFMat4x4(viewProjection);
		}

		MATRIX GetRotationMat()const {
			return LoadFMat3x3(rotationMat);
		}
	};

	struct LoadModelComponent
	{
		ResPtr<Model> model;
	};

	struct MaterialComponent
	{
		ResPtr<Material> material;
		U32 materialIndex = 0;
	};

	struct MeshComponent
	{
		ResPtr<Model> model;
		Mesh* mesh = nullptr;
		U32 geometryOffset = 0;
	};

	struct ObjectComponent
	{
		F32x3 center = F32x3(0, 0, 0);
		AABB aabb;
		U32 index = 0;
		U8 stencilRef = 0;

		// Runtime infos
		ECS::Entity mesh = ECS::INVALID_ENTITY;
		FMat4x4 worldMat = IDENTITY_MATRIX;
	};

	struct LightComponent
	{
		enum LightType
		{
			DIRECTIONAL,
			POINT,
			SPOT,
			LIGHTTYPE_COUNT,
		};
		LightType type = POINT;
		F32x3 position;
		F32x4 rotation;
		F32x3 scale;
		F32x3 direction;

		F32x3 color = F32x3(1.0f);
		F32 intensity = 1.0f;
		F32 range = 10.0f;
		AABB aabb;
	};

	class VULKAN_TEST_API RenderPassPlugin
	{
	public:
		virtual ~RenderPassPlugin() = default;

		virtual void Update(F32 dt) = 0;
		virtual void SetupRenderPasses(RenderGraph& graph) = 0;
		virtual void SetupDependencies(RenderGraph& graph) = 0;
		virtual void SetupResources(RenderGraph& graph) = 0;
	};

	struct RenderPassComponent
	{
		RenderPassPlugin* renderPassPlugin = nullptr;
	};
	
	class VULKAN_TEST_API RenderScene : public IScene
	{
	public:
		template<typename T>
		using EntityMap = std::unordered_map<T, ECS::Entity>;

		static UniquePtr<RenderScene> CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world);
		static void Reflect(World* world);

		virtual void UpdateVisibility(struct Visibility& vis) = 0;
		virtual void UpdateRenderData(GPU::CommandList& cmd) = 0;
		virtual const ShaderSceneCB& GetShaderScene()const = 0;

		virtual PickResult CastRayPick(const Ray& ray, U32 mask = ~0) = 0;

		virtual void CreateComponent(ECS::Entity entity, ECS::EntityID compID) = 0;

		// Entity
		virtual ECS::Entity CreateEntity(const char* name) = 0;
		virtual void DestroyEntity(ECS::Entity entity) = 0;

		// Camera
		virtual CameraComponent* GetMainCamera() = 0;

		// Model
		virtual void LoadModel(const char* name, const Path& path) = 0;

		// Mesh
		virtual ECS::Entity CreateMesh(ECS::Entity entity) = 0;

		// Material
		virtual ECS::Entity CreateMaterial(ECS::Entity entity) = 0;

		// Object
		virtual ECS::Entity CreateObject(ECS::Entity entity) = 0;
		virtual void ForEachObjects(std::function<void(ECS::Entity, ObjectComponent&)> func) = 0;

		// Light
		virtual ECS::Entity CreatePointLight(ECS::Entity entity) = 0;
		virtual ECS::Entity CreateLight(
			ECS::Entity entity,
			LightComponent::LightType type = LightComponent::LightType::POINT,
			const F32x3 pos = F32x3(1.0f), 
			const F32x3 color = F32x3(1.0f), 
			F32 intensity = 1.0f, 
			F32 range = 10.0f) = 0;
		virtual void ForEachLights(std::function<void(ECS::Entity, LightComponent&)> func) = 0;
	};
}