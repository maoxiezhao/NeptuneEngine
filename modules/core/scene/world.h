#pragma once

#include "core\common.h"
#include "core\engine.h"
#include "core\plugin\plugin.h"
#include "core\memory\memory.h"
#include "core\utils\delegate.h"
#include "core\collections\hashMap.h"
#include "ecs\ecs\ecs.hpp"

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

	class VULKAN_TEST_API World : public ECS::World
	{
	public:
		explicit World(Engine* engine_);
		virtual ~World();

		static void SetupWorld();

		ECS::Entity CreateEntity(const char* name);
		void DeleteEntity(ECS::Entity entity);

		IScene* GetScene(const char* name)const;
		void AddScene(UniquePtr<IScene>&& scene);
		std::vector<UniquePtr<IScene>>& GetScenes();

		DelegateList<void(ECS::Entity)>& EntityCreated() { return entityCreated; }
		DelegateList<void(ECS::Entity)>& EntityDestroyed() { return entityDestroyed; }

	private:
		Engine* engine;
		std::vector<UniquePtr<IScene>> scenes;

		DelegateList<void(ECS::Entity)> entityCreated;
		DelegateList<void(ECS::Entity)> entityDestroyed;
	};

	template<>
	struct HashMapHashFunc<ECS::Entity>
	{
		static U32 Get(ECS::Entity key)
		{
			// https://xoshiro.di.unimi.it/splitmix64.c
			U64 x = key;
			x ^= x >> 30;
			x *= 0xbf58476d1ce4e5b9U;
			x ^= x >> 27;
			x *= 0x94d049bb133111ebU;
			x ^= x >> 31;
			return U32((x >> 32) ^ x);
		}
	};
}