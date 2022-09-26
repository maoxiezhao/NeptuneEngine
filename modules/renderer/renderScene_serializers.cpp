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
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, TransformComponent& v)
			{
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
	}

	template<typename T>
	inline void SerializeComponent(ISerializable::SerializeStream& stream, ECS::Entity entity, T& comp)
	{
		stream.StartObject();
		stream.Key("Entity");
		stream.String(entity.GetPath());
		stream.Key("Component");
		stream.StartObject();
		Serialization::Serialize(stream, comp, nullptr);
		stream.EndObject();
		stream.EndObject();
	}

	template<typename T>
	inline void SerializeComponents(ISerializable::SerializeStream& stream, const char* name)
	{
		stream.StartObject();
		{
			stream.Key(name);
			stream.StartArray();
			


			stream.EndArray();
		}
		stream.EndObject();
	}

	template<typename T>
	inline void DeserializeComponent(ISerializable::DeserializeStream& stream, World* world)
	{
		auto path = JsonUtils::GetString(stream, "Entity");
		auto entity = world->Entity(path);
		entity.Add<T>();
		auto comp = entity.GetMut<T>();
		if (comp != nullptr)
			Serialization::Deserialize(stream, *comp);
	}

	template<typename T>
	inline void DeserializeComponents(ISerializable::DeserializeStream& stream, World* world, const char* name)
	{
		auto compIt = stream.FindMember("Lights");
		if (compIt != stream.MemberEnd())
		{
			auto& compDatas = compIt->value;
			for (int i = 0; i < (I32)compDatas.Size(); i++)
				DeserializeComponent<T>(compDatas[i], world);
		}
	}

	void RenderScene::Serialize(SerializeStream& stream, const void* otherObj)
	{
		stream.StartObject();

		// Entities
		stream.JKEY("Entities");
		stream.StartArray();
		ForEachEntity([&](ECS::Entity entity, RenderComponentTag& comp){
			stream.Entity(entity);
		});
		stream.EndArray();

		// Components
		stream.JKEY("Components");
		SerializeComponents<TransformComponent>(stream, "Transforms");
		SerializeComponents<LightComponent>(stream, "Lights");
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
			DeserializeComponents<LightComponent>(it->value, world, "Lights");
		}
	}
}