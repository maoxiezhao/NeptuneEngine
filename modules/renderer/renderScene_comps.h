#pragma once

#include "rendererCommon.h"
#include "shaderInterop_renderer.h"
#include "renderGraph.h"
#include "content\resources\model.h"

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
		FMat4x4 invProjection;
		FMat3x3 rotationMat;

		Frustum frustum;

		GPU::BindlessDescriptorPtr textureDepthBindless;
		GPU::BindlessDescriptorPtr bufferLightTileBindless;

		void UpdateCamera();
		void UpdateTransform(const Transform& transform);

		MATRIX GetViewProjection() const {
			return LoadFMat4x4(viewProjection);
		}

		MATRIX GetInvProjection() const {
			return LoadFMat4x4(invProjection);
		}

		MATRIX GetRotationMat() const {
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
		AABB aabb;
		U32 subsetsPerLod = 0;
		I32 lodsCount = 0;
		Mesh* meshes[Model::MAX_MODEL_LODS];
		U32 geometryOffset = 0;
	};

	struct ObjectComponent
	{
		AABB aabb;
		U8 stencilRef = 0;

		// Runtime
		U32 objectIndex = 0;
		FMat4x4 worldMat = IDENTITY_MATRIX;
		F32x3 center = F32x3(0, 0, 0);
		F32 radius = 0.0f;
		I32 lodIndex = 0;
		ECS::Entity mesh = ECS::INVALID_ENTITY;
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
}