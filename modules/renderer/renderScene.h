#pragma once

#include "rendererCommon.h"
#include "renderScene_comps.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderScene : public IScene
	{
	public:
		template<typename T>
		using EntityMap = std::unordered_map<T, ECS::Entity>;

		static UniquePtr<RenderScene> CreateScene(RendererPlugin& rendererPlugin, Engine& engine, World& world);
		static void Reflect();

		virtual void UpdateVisibility(struct Visibility& vis) = 0;
		virtual void UpdateRenderData(GPU::CommandList& cmd) = 0;
		virtual const ShaderSceneCB& GetShaderScene()const = 0;

		virtual PickResult CastRayPick(const Ray& ray, U32 mask = ~0) = 0;

		virtual void CreateComponent(ECS::Entity entity, ComponentType compType) = 0;

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
		virtual ECS::Entity CreateDirectionLight(ECS::Entity entity) = 0;
		virtual ECS::Entity CreateSpotLight(ECS::Entity entity) = 0;
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