#pragma once

#include "core\common.h"
#include "core\scene\world.h"
#include "core\collections\array.h"
#include "math\geometry.h"
#include "math\math.hpp"
#include "renderGraph.h"
#include "shaderInterop.h"
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
	};

	struct MeshComponent
	{
		ResPtr<Model> model;
		Mesh* mesh = nullptr;
		U32 meshCount = 0;
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

		virtual void UpdateVisibility(struct Visibility& vis) = 0;
		virtual void UpdateRenderData(GPU::CommandList& cmd) = 0;

		virtual const ShaderSceneCB& GetShaderScene()const = 0;

		// Camera
		virtual CameraComponent* GetMainCamera() = 0;

		// MeshInstance
		virtual ECS::EntityID CreateMeshInstance(const char* name, const Path& path) = 0;
		virtual void ForEachMeshes(std::function<void(ECS::EntityID, MeshComponent&)> func) = 0;

		template<typename C>
		C* GetComponent(ECS::EntityID entity)
		{
			return GetWorld().GetComponent<C>(entity);
		}
	};
}