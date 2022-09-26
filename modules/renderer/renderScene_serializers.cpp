#include "RenderScene.h"
#include "renderer.h"
#include "core\scene\reflection.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\serialization.h"

namespace VulkanTest
{
	namespace Serialization
	{
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
				DESERIALIZE_MEMBER("Type", v.color);
				DESERIALIZE_MEMBER("Intensity", v.color);
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
		Serialization::Serialize(stream, comp, nullptr);
		stream.EndObject();
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
		stream.StartObject();
		{
			stream.JKEY("Lights");
			stream.StartArray();
			ForEachLights([&](ECS::Entity entity, LightComponent& comp) {
				SerializeComponent(stream, entity, comp);
			});
			stream.EndArray();
		}
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
	}
}