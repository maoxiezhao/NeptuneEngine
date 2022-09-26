#include "serialization.h"
#include "jsonUtils.h"

namespace VulkanTest
{
namespace Serialization
{
	void DeserializeEntity(ISerializable::DeserializeStream& stream, World* world, ECS::Entity* entityOut)
	{
		auto path = JsonUtils::GetString(stream, "Path");
		if (path.empty())
			return;

		ECS::Entity entity = world->Entity(path.c_str());
		if (entityOut)
			*entityOut = entity;
	}
}
}