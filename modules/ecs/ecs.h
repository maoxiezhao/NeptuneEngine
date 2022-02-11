#pragma once

#include "core\common.h"

// TODO: it's used to learn ecs, we will replace it with flecs.
namespace VulkanTest
{
namespace ECS
{
	using EntityID = U32;
	static const EntityID INVALID_ENTITY = 0;

	class World
	{
	public:
		inline EntityID CreateEntityID()
		{
			static std::atomic<EntityID> next{ INVALID_ENTITY + 1 };
			return next.fetch_add(1);
		}
	};
}
}