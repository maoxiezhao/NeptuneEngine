#include "RenderScene.h"
#include "renderer.h"
#include "core\scene\reflection.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\serialization.h"
#include "core\serialization\jsonUtils.h"

namespace VulkanTest
{
	namespace Serialization
	{
		template<>
		struct SerializeTypeNormalMapping<TransformComponent> : SerializeTypeBase<TransformComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const TransformComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(TransformComponent);	
				SERIALIZE_OBJECT_MEMBER("Scale", v, transform.scale);
				SERIALIZE_OBJECT_MEMBER("Rotation", v, transform.rotation);
				SERIALIZE_OBJECT_MEMBER("Translation", v, transform.translation);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, TransformComponent& v)
			{
				DESERIALIZE_MEMBER("Scale", v.transform.scale);
				DESERIALIZE_MEMBER("Rotation", v.transform.rotation);
				DESERIALIZE_MEMBER("Translation", v.transform.translation);
				v.transform.SetDirty();
				v.transform.UpdateTransform();
			}
		};

		template<>
		struct SerializeTypeNormalMapping<LightComponent> : SerializeTypeBase<LightComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const LightComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(LightComponent);
				SERIALIZE_OBJECT_MEMBER("Color", v, color);
				SERIALIZE_OBJECT_MEMBER("Type", v, type);
				SERIALIZE_OBJECT_MEMBER("Intensity", v, intensity);
				SERIALIZE_OBJECT_MEMBER("Range", v, range);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, LightComponent& v)
			{
				DESERIALIZE_MEMBER("Color", v.color);
				DESERIALIZE_MEMBER("Type", v.type);
				DESERIALIZE_MEMBER("Intensity", v.intensity);
				DESERIALIZE_MEMBER("Range", v.range);
			}
		};

		template<>
		struct SerializeTypeNormalMapping<MaterialComponent> : SerializeTypeBase<MaterialComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const MaterialComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(MaterialComponent);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, MaterialComponent& v)
			{
			}
		};

		template<>
		struct SerializeTypeNormalMapping<MeshComponent> : SerializeTypeBase<MeshComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const MeshComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(MeshComponent);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, MeshComponent& v)
			{
			}
		};

		template<>
		struct SerializeTypeNormalMapping<ObjectComponent> : SerializeTypeBase<ObjectComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const ObjectComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(ObjectComponent);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, ObjectComponent& v)
			{
			}
		};
	}

	template<typename T>
	inline void SerializeComponents(ISerializable::SerializeStream& stream, World* world, const char* name)
	{
		stream.Key(name);
		stream.StartArray();
		world->Each<T>([&](ECS::Entity entity, T& comp)
		{
			stream.StartObject();
			stream.Key("Entity");
			stream.String(entity.GetPath());
			stream.Key("Component");
			stream.StartObject();
			Serialization::Serialize(stream, comp, nullptr);
			stream.EndObject();
			stream.EndObject();
		});
		stream.EndArray();
	}

	template<typename T>
	inline void DeserializeComponents(ISerializable::DeserializeStream& stream, World* world, const char* name)
	{
		auto compIt = stream.FindMember(name);
		if (compIt != stream.MemberEnd())
		{
			auto& compDatas = compIt->value;
			for (int i = 0; i < (I32)compDatas.Size(); i++)
			{
				auto path = JsonUtils::GetString(compDatas[i], "Entity");
				if (path.empty())
					continue;

				auto compData = compDatas[i].FindMember("Component");
				if (compData != compDatas[i].MemberEnd())
				{
					auto entity = world->Entity(path);
					entity.Add<T>();
					auto comp = entity.GetMut<T>();
					if (comp != nullptr)
						Serialization::Deserialize(compData->value, *comp);
				}
			}
		}
	}

	void RenderScene::Serialize(SerializeStream& stream, const void* otherObj)
	{
		auto world = &GetWorld();
		ASSERT(world != nullptr);

		stream.StartObject();

		// Entities
		stream.JKEY("Entities");
		stream.StartArray();
		world->Each<RenderComponentTag>([&](ECS::Entity entity, RenderComponentTag& comp) {
			stream.Entity(entity);
		});
		stream.EndArray();

		// Components
		stream.JKEY("Components");
		stream.StartObject();
		SerializeComponents<TransformComponent>(stream, world, "Transforms");
		SerializeComponents<MaterialComponent>(stream, world, "Materials");
		SerializeComponents<MeshComponent>(stream, world, "Meshes");
		SerializeComponents<ObjectComponent>(stream, world, "Objects");
		SerializeComponents<LightComponent>(stream, world, "Lights");
		stream.EndObject();

		stream.EndObject();
	}

	void RenderScene::Deserialize(DeserializeStream& stream)
	{
		auto world = &GetWorld();
		ASSERT(world != nullptr);

		// Entities
		{
			auto it = stream.FindMember("Entities");
			if (it == stream.MemberEnd())
				return;

			auto& data = it->value;
			if (!data.IsArray())
				return;

			for (int i = 0; i < data.Size(); i++)
			{
				auto& entityData = data[i];
				if (entityData.IsObject())
				{
					Serialization::DeserializeEntity(entityData, world, nullptr);
				}
			}
		}

		// Components
		{
			auto it = stream.FindMember("Components");
			if (it == stream.MemberEnd())
				return;

			DeserializeComponents<TransformComponent>(it->value, world, "Transforms");
			DeserializeComponents<MaterialComponent>(it->value, world, "Materials");
			DeserializeComponents<MeshComponent>(it->value, world, "Meshes");
			DeserializeComponents<ObjectComponent>(it->value, world, "Objects");
			DeserializeComponents<LightComponent>(it->value, world, "Lights");
		}
	}
}