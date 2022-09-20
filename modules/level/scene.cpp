#include "scene.h"
#include "core\serialization\jsonWriteStream.h"

namespace VulkanTest
{
	Scene::Scene(World& world) :
		Scene(world, ScriptingObjectParams(Guid::New()))
	{
	}

	Scene::Scene(World& world_, const ScriptingObjectParams& params) :
		ScriptingObject(params),
		world(world_)
	{
	}

	void Scene::Serialize(SerializeStream& stream, const void* otherObj)
	{
	}

	void Scene::Deserialize(DeserializeStream& stream)
	{
	}
}