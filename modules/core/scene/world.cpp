#include "world.h"
#include "core\utils\string.h"

namespace VulkanTest
{
	ISystem::ISystem(IScene& scene_) :
		scene(scene_),
		system(ECS::INVALID_ENTITY)
	{
	}

	ISystem::~ISystem()
	{
		if (system != ECS::INVALID_ENTITY)
			scene.GetWorld().RemoveSystem(system);
	}

	void ISystem::UpdateSystem()
	{
		if (system != ECS::INVALID_ENTITY)
			scene.GetWorld().RunSystem(system);
	}

	World::World(Engine* engine_) :
		engine(engine_)
	{
		world = ECS::World::Create();
	}

	World::~World()
	{
		world.reset();
	}

	const ECS::EntityBuilder& World::CreateEntity(const char* name)
	{
		return world->CreateEntity(name);
	}

	const ECS::EntityBuilder& World::CreatePrefab(const char* name)
	{
		return world->CreatePrefab(name);
	}

	ECS::EntityID World::CreateEntityID(const char* name)
	{
		return world->CreateEntityID(name);
	}

	ECS::EntityID World::FindEntity(const char* name)
	{
		return world->FindEntityIDByName(name);
	}

	ECS::EntityID World::EntityExists(ECS::EntityID entity) const
	{
		return world->EntityExists(entity);
	}

	void World::DeleteEntity(ECS::EntityID entity)
	{
		return world->DeleteEntity(entity);
	}

	void World::SetEntityName(ECS::EntityID entity, const char* name)
	{
		world->SetEntityName(entity, name);
	}

	bool World::HasComponent(ECS::EntityID entity, ECS::EntityID compID)
	{
		return world->HasComponent(entity, compID);
	}

	void World::RunSystem(ECS::EntityID system)
	{
		world->RunSystem(system);
	}

	void World::RemoveSystem(ECS::EntityID system)
	{
		// TODO
	}

	IScene* World::GetScene(const char* name) const
	{
		for (auto& scene : scenes)
		{
			if (EqualString(scene->GetPlugin().GetName(), name))
				return scene.Get();
		}
		return nullptr;
	}

	void World::AddScene(UniquePtr<IScene>&& scene)
	{
		scenes.push_back(scene.Move());
	}

	std::vector<UniquePtr<IScene>>& World::GetScenes()
	{
		return scenes;
	}
}