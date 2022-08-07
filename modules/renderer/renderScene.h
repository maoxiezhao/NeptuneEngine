#pragma once

#include "core\common.h"
#include "core\scene\world.h"
#include "core\collections\array.h"
#include "math\geometry.h"
#include "math\math.hpp"
#include "renderGraph.h"
#include "shaderInterop_renderer.h"
#include "enums.h"
#include "model.h"

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

		Frustum frustum;

		void UpdateCamera();
		void UpdateTransform(const Transform& transform);

		MATRIX GetViewProjection() const {
			return LoadFMat4x4(viewProjection);
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
		ECS::EntityID mesh = ECS::INVALID_ENTITY;
		F32x3 center = F32x3(0, 0, 0);
		AABB aabb;
		U32 index = 0;
		U8 stencilRef = 1;
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
		using EntityMap = std::unordered_map<T, ECS::EntityID>;

		static UniquePtr<RenderScene> CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world);
		static void Reflect(World* world);

		virtual void UpdateVisibility(struct Visibility& vis) = 0;
		virtual void UpdateRenderData(GPU::CommandList& cmd) = 0;
		virtual const ShaderSceneCB& GetShaderScene()const = 0;

		// Entity
		virtual ECS::EntityID CreateEntity(const char* name) = 0;
		virtual void DestroyEntity(ECS::EntityID entity) = 0;

		// Camera
		virtual CameraComponent* GetMainCamera() = 0;

		// Model
		virtual void LoadModel(const char* name, const Path& path) = 0;

		// Mesh
		virtual ECS::EntityID CreateMesh(const char* name) = 0;

		// Material
		virtual ECS::EntityID CreateMaterial(const char* name) = 0;

		// Object
		virtual ECS::EntityID CreateObject(const char* name) = 0;
		virtual void ForEachObjects(std::function<void(ECS::EntityID, ObjectComponent&)> func) = 0;

		template<typename C>
		C* GetComponent(ECS::EntityID entity)
		{
			return GetWorld().GetComponent<C>(entity);
		}
	};
}