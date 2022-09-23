#include "scene.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\serialization.h"

namespace VulkanTest
{
	Scene::Scene() :
		Scene(ScriptingObjectParams(Guid::New()))
	{
	}

	Scene::Scene(const ScriptingObjectParams& params) :
		ScriptingObject(params)
	{
		world = &Engine::Instance->CreateWorld();
	}

	Scene::~Scene()
	{
		Engine::Instance->DestroyWorld(world);
	}

	void Scene::Start()
	{
#ifndef CJING3D_EDITOR
		if (!isPlaying)
		{
			Engine::Instance->Start(world);
			isPlaying = true;
		}
#endif
	}

	void Scene::Stop()
	{
#ifndef CJING3D_EDITOR
		if (isPlaying)
		{
			Engine::Instance->Stop(world);
			isPlaying = false;
		}
#endif
	}

	void Scene::Serialize(SerializeStream& stream, const void* otherObj)
	{
		SERIALIZE_GET_OTHER_OBJ(Scene);
		stream.StartObject();
		stream.JKEY("ID");
		stream.Guid(GetGUID());
		SERIALIZE_MEMBER(Name, name);
		stream.EndObject();
	}

	void Scene::Deserialize(DeserializeStream& stream)
	{
		DESERIALIZE_MEMBER(Name, name);
	}
}