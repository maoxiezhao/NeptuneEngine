#pragma once

#include "core\common.h"
#include "core\engine.h"
#include "core\plugin\plugin.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "ecs\ecs\ecs.h"

namespace VulkanTest
{
	class World;

	struct VULKAN_TEST_API IScene
	{
		virtual ~IScene() {}

		virtual void Init() = 0;
		virtual void Uninit() = 0;
		virtual void Update(float dt, bool paused) = 0;
		virtual void Clear() = 0;
		virtual IPlugin& GetPlugin()const = 0;
		virtual World& GetWorld() = 0;
	};

	class VULKAN_TEST_API ISystem
	{
	public:
		ISystem(IScene& scene_);
		virtual ~ISystem();
		void UpdateSystem();

	protected:
		IScene& scene;
		ECS::EntityID system;
	};

	class VULKAN_TEST_API World
	{
	public:
		explicit World(Engine* engine_);
		virtual ~World();

		const ECS::EntityBuilder& CreateEntity(const char* name);
		const ECS::EntityBuilder& CreatePrefab(const char* name);
		void GetValidEntityName(char(&out)[64]);
		ECS::EntityID CreateEntityID(const char* name);
		ECS::EntityID FindEntity(const char* name);
		ECS::EntityID EntityExists(ECS::EntityID entity)const;
		ECS::EntityID GetEntityParent(ECS::EntityID entity);
		void DeleteEntity(ECS::EntityID entity);
		void SetEntityName(ECS::EntityID entity, const char* name);
		const char* GetEntityName(ECS::EntityID entity);
		bool HasComponent(ECS::EntityID entity, ECS::EntityID compID);

		template<typename C>
		bool HasComponent(ECS::EntityID entity)
		{
			return world->HasComponent<C>(entity);
		}

		template<typename C>
		ECS::EntityID GetComponentID()
		{
			return ECS::ComponentType<C>::ID(*world);
		}

		template<typename C>
		C* GetComponent(ECS::EntityID entity)
		{
			return world->GetComponent<C>(entity);
		}

		template<typename C>
		C* GetSingletonComponent()
		{
			return world->GetSingletonComponent<C>();
		}

		template <typename Func>
		void EachChildren(ECS::EntityID entity, Func&& func)
		{
			world->EachChildren(entity, ECS_MOV(func));
		}

		template<typename C>
		void AddComponent(ECS::EntityID entity)
		{
			world->AddComponent<C>(entity);
		}

		template<typename T, typename Func>
		void SetComponenetOnAdded(Func&& func)
		{
			world->SetComponenetOnAdded(ECS_MOV(func));
		}

		template<typename T, typename Func>
		void SetComponenetOnRemoved(Func&& func)
		{
			world->SetComponenetOnRemoved<T>(ECS_MOV(func));
		}

		template<typename... Comps>
		inline ECS::QueryBuilder<Comps...> CreateQuery()
		{
			return ECS::QueryBuilder<Comps...>(world.get());
		}

		template<typename... Args>
		ECS::SystemBuilder<Args...> CreateSystem()
		{
			return world->CreateSystem<Args...>();
		}
		void RunSystem(ECS::EntityID system);
		void RemoveSystem(ECS::EntityID system);

		IScene* GetScene(const char* name)const;
		void AddScene(UniquePtr<IScene>&& scene);
		std::vector<UniquePtr<IScene>>& GetScenes();

		DelegateList<void(ECS::EntityID)>& EntityCreated() { return entityCreated; }
		DelegateList<void(ECS::EntityID)>& EntityDestroyed() { return entityDestroyed; }

	private:
		Engine* engine;
		ECS_UNIQUE_PTR<ECS::World> world;
		std::vector<UniquePtr<IScene>> scenes;

		DelegateList<void(ECS::EntityID)> entityCreated;
		DelegateList<void(ECS::EntityID)> entityDestroyed;
	};
}