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

			static void Deserialize(ISerializable::DeserializeStream& stream, TransformComponent& v, World* world)
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

			static void Deserialize(ISerializable::DeserializeStream& stream, LightComponent& v, World* world)
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
				stream.JKEY("Materials");
				stream.StartArray();
				for (const auto& material : v.materials)
				{
					Serialization::Serialize(stream, material, other);
				}
				stream.EndArray();
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, MaterialComponent& v, World* world)
			{
				auto mateiralIt = stream.FindMember("Materials");
				if (mateiralIt != stream.MemberEnd())
				{
					auto& materialData = mateiralIt->value;
					v.materials.resize(materialData.Size());
					for (I32 i = 0; i < (I32)materialData.Size(); i++)
						Serialization::Deserialize(materialData[i], v.materials[i]);
				}
			}
		};

		template<>
		struct SerializeTypeNormalMapping<MeshComponent> : SerializeTypeBase<MeshComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const MeshComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(MeshComponent);
				SERIALIZE_OBJECT_MEMBER("Model", v, model);
				SERIALIZE_OBJECT_MEMBER("LodsCount", v, lodsCount);
				SERIALIZE_OBJECT_MEMBER("MeshCount", v, meshCount);
				stream.JKEY("MeshInfos");
				stream.StartArray();
				for (int i = 0; i < v.lodsCount; i++)
				{
					stream.StartArray();
					for (const auto& mesh : v.meshes[i])
					{
						SERIALIZE_GET_OTHER_OBJ(MeshComponent::MeshInfo);
						stream.StartObject();
						SERIALIZE_OBJECT_MEMBER("MeshIndex", mesh, meshIndex);
						SERIALIZE_OBJECT_MEMBER("AABB", mesh, aabb);
						stream.JKEY("Material");
						Serialization::SerializeEntity(stream, mesh.material);
						stream.EndObject();
					}
					stream.EndArray();
				}
				stream.EndArray();
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, MeshComponent& v, World* world)
			{
				DESERIALIZE_MEMBER("Model", v.model);
				DESERIALIZE_MEMBER("LodsCount", v.lodsCount);
				DESERIALIZE_MEMBER("MeshCount", v.meshCount);
				auto meshInfosIt = stream.FindMember("MeshInfos");
				if (meshInfosIt != stream.MemberEnd())
				{
					for (I32 lodIndex = 0; lodIndex < (I32)meshInfosIt->value.Size(); lodIndex++)
					{
						auto& meshInfoDatas = meshInfosIt->value[lodIndex];
						auto& meshInfos = v.meshes[lodIndex];
						meshInfos.resize(meshInfoDatas.Size());
						for (I32 i = 0; i < (I32)meshInfoDatas.Size(); i++)
						{
							DESERIALIZE_MEMBER_WITH("MeshIndex", meshInfos[i].meshIndex, meshInfoDatas[i]);
							DESERIALIZE_MEMBER_WITH("AABB", meshInfos[i].aabb, meshInfoDatas[i]);
							
							auto it = meshInfoDatas[i].FindMember("Material");
							if (it != meshInfoDatas[i].MemberEnd())
								meshInfos[i].material = Serialization::DeserializeEntity(it->value, world);
						}
					}
				}
			}
		};

		template<>
		struct SerializeTypeNormalMapping<ObjectComponent> : SerializeTypeBase<ObjectComponent>
		{
			static void Serialize(ISerializable::SerializeStream& stream, const ObjectComponent& v, const void* otherObj)
			{
				SERIALIZE_GET_OTHER_OBJ(ObjectComponent);
				SERIALIZE_OBJECT_MEMBER("AABB", v, aabb);
				SERIALIZE_OBJECT_MEMBER("Stencil", v, stencilRef);
				stream.JKEY("Mesh");
				Serialization::SerializeEntity(stream, v.mesh);
			}

			static void Deserialize(ISerializable::DeserializeStream& stream, ObjectComponent& v, World* world)
			{
				DESERIALIZE_MEMBER("AABB", v.aabb);
				DESERIALIZE_MEMBER("Stencil", v.stencilRef);
				auto it = stream.FindMember("Mesh");
				if (it != stream.MemberEnd())
					v.mesh = Serialization::DeserializeEntity(it->value, world);
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
			Serialization::SerializeEntity(stream, entity);
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
						Serialization::Deserialize(compData->value, *comp, world);
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

			for (I32 i = 0; i < (I32)data.Size(); i++)
			{
				auto& entityData = data[i];
				if (entityData.IsObject())
				{
					ECS::Entity entity = ECS::INVALID_ENTITY;
					auto pathData = entityData.FindMember("Path");
					if (pathData != entityData.MemberEnd())
						entity = Serialization::DeserializeEntity(pathData->value, world);
					
					if (entity != ECS::INVALID_ENTITY)
					{
						auto parentData = entityData.FindMember("Parent");
						if (parentData != entityData.MemberEnd())
							entity.ChildOf(Serialization::DeserializeEntity(parentData->value, world));
					}
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