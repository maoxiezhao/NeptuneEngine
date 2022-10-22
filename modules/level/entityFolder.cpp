#ifdef CJING3D_EDITOR

#include "entityFolder.h"
#include "core\serialization\jsonUtils.h"
#include "core\serialization\jsonWriter.h"
#include "core\serialization\serialization.h"

namespace VulkanTest
{
	EntityFolder::EntityFolder(World& world_) :
		world(world_)
	{
		world.EntityCreated().Bind<&EntityFolder::OnEntityCreated>(this);
		world.EntityDestroyed().Bind<&EntityFolder::OnEntityDestroyed>(this);

		const FolderID rootID = folderPool.Alloc();
		Folder& root = folderPool.Get(rootID);
		CopyString(root.name, "root");
		selectedFolder = rootID;
	}

	EntityFolder::~EntityFolder()
	{
		world.EntityCreated().Unbind<&EntityFolder::OnEntityCreated>(this);
		world.EntityDestroyed().Unbind<&EntityFolder::OnEntityDestroyed>(this);
	}

	void EntityFolder::Serialize(SerializeStream& stream, const void* otherObj)
	{
		stream.StartObject();
		stream.JKEY("Entities");
		{
			stream.StartArray();
			for (auto it = entities.begin(); it != entities.end(); ++it)
			{
				if (it.key() == ECS::INVALID_ENTITY)
					continue;

				auto& item = it.value();
				stream.StartObject();
				stream.JKEY("Entity");
				stream.String(it.key().GetPath());
				stream.JKEY("FolderID");
				stream.Uint((U32)item.folder);

				if (item.next != ECS::INVALID_ENTITY)
				{
					stream.JKEY("Next");
					stream.String(item.next.GetPath());
				}

				if (item.prev != ECS::INVALID_ENTITY)
				{
					stream.JKEY("Prev");
					stream.String(item.prev.GetPath());
				}

				stream.EndObject();
			}
			stream.EndArray();
		}

		stream.JKEY("FolderPool");
		{
			SERIALIZE_GET_OTHER_OBJ(Folder);
			stream.StartObject();
			stream.JKEY("FirstFree");
			stream.Int(folderPool.firstFree);
			stream.JKEY("Data");
			stream.StartArray();
			for (const auto& folder : folderPool.data)
			{
				stream.StartObject();
				stream.JKEY("Name");
				stream.String(folder.name);
				SERIALIZE_OBJECT_MEMBER("Parent", folder, parentFolder);
				SERIALIZE_OBJECT_MEMBER("Child", folder, childFolder);
				SERIALIZE_OBJECT_MEMBER("Next", folder, nextFolder);
				SERIALIZE_OBJECT_MEMBER("Prev", folder, prevFolder);
				stream.JKEY("FirstEntity");
				stream.String(folder.firstEntity != ECS::INVALID_ENTITY ? folder.firstEntity.GetPath() : "");
				stream.EndObject();
			}
			stream.EndArray();
			stream.EndObject();
		}

		stream.EndObject();
	}

	void EntityFolder::Deserialize(DeserializeStream& stream)
	{
		auto entitiesDataIt = stream.FindMember("Entities");
		if (entitiesDataIt != stream.MemberEnd())
		{
			auto& entitiesData = entitiesDataIt->value;
			for (int i = 0; i < entitiesData.Size(); i++)
			{
				auto& entityData = entitiesData[i];
				auto entityPath = JsonUtils::GetString(entityData, "Entity");
				auto entity = world.Entity(entityPath);
				if (entity == ECS::INVALID_ENTITY)
					continue;

				EntityItem entityItem = {};
				entityItem.folder = (U16)JsonUtils::GetUint(entityData, "FolderID", 0);
				entityItem.next = JsonUtils::GetEntity(entityData, "Next", &world);
				entityItem.prev = JsonUtils::GetEntity(entityData, "Prev", &world);
				entities.insert(entity, entityItem);
			}
		}

		auto folderPoolDataIt = stream.FindMember("FolderPool");
		if (folderPoolDataIt != stream.MemberEnd())
		{
			auto& folderPoolData = folderPoolDataIt->value;
			folderPool.firstFree = JsonUtils::GetInt(folderPoolData, "FirstFree", -1);

			auto folderDatasIt = folderPoolData.FindMember("Data");
			if (folderDatasIt != folderPoolData.MemberEnd())
			{
				auto& folderDatas = folderDatasIt->value;
				folderPool.data.resize(folderDatas.Size());
				for (int i = 0; i < folderDatas.Size(); i++)
				{
					auto& folderData = folderDatas[i];
					Folder& Folder = folderPool.data[i];
					DESERIALIZE_MEMBER_WITH("Parent", Folder.parentFolder, folderData);
					DESERIALIZE_MEMBER_WITH("Child", Folder.childFolder, folderData);
					DESERIALIZE_MEMBER_WITH("Next", Folder.nextFolder, folderData);
					DESERIALIZE_MEMBER_WITH("Prev", Folder.prevFolder, folderData);
					Folder.firstEntity = JsonUtils::GetEntity(folderData, "FirstEntity", &world);
					String name = JsonUtils::GetString(folderData, "Name");
					memset(Folder.name, 0, sizeof(Folder.name));
					memcpy(Folder.name, name.c_str(), name.size());
				}
			}
		}
	}

	void EntityFolder::OnEntityCreated(ECS::Entity e)
	{
		MoveToFolder(e, selectedFolder);
	}

	void EntityFolder::OnEntityDestroyed(ECS::Entity e)
	{
		Folder& parent = GetFolder(entities[e].folder);
		EntityItem& entity = entities[e];
		if (parent.firstEntity == e)
			parent.firstEntity = entity.next;

		if (entity.prev != ECS::INVALID_ENTITY)
			entities[entity.prev].next = entity.next;

		if (entity.next != ECS::INVALID_ENTITY)
			entities[entity.next].prev = entity.prev;

		entity.folder = INVALID_FOLDER;
		entity.next = ECS::INVALID_ENTITY;
		entity.prev = ECS::INVALID_ENTITY;
	}

	ECS::Entity EntityFolder::GetNextEntity(ECS::Entity e) const
	{
		auto it = entities.find(e);
		return it.isValid() ? it.value().next : ECS::INVALID_ENTITY;
	}

	void EntityFolder::MoveToFolder(ECS::Entity e, FolderID folderID)
	{
		ASSERT(folderID != INVALID_FOLDER);
		auto it = entities.find(e);
		if (!it.isValid())
			it = entities.emplace(e);

		EntityItem& item = it.value();
		// Remove from previous folder
		if (item.folder != INVALID_FOLDER)
			RemoveFromFolder(e, item.folder);

		item.folder = folderID;

		Folder& folder = GetFolder(folderID);
		item.next = folder.firstEntity;
		item.prev = ECS::INVALID_ENTITY;
		folder.firstEntity = e;

		if (item.next != ECS::INVALID_ENTITY)
			entities[item.next].prev = e;
	}

	void EntityFolder::RemoveFromFolder(ECS::Entity e, FolderID folderID)
	{
		ASSERT(folderID != INVALID_FOLDER);
		auto it = entities.find(e);
		if (!it.isValid())
			return;

		EntityItem& item = it.value();
		if (item.folder != INVALID_FOLDER)
		{
			Folder& f = folderPool.Get(item.folder);
			if (f.firstEntity == e)
				f.firstEntity = item.next;

			if (item.prev != ECS::INVALID_ENTITY)
				entities[item.prev].next = item.next;

			if (item.next != ECS::INVALID_ENTITY)
				entities[item.next].prev = item.prev;

			item.folder = INVALID_FOLDER;
		}
	}

	EntityFolder::FolderID EntityFolder::EmplaceFolder(FolderID parent)
	{
		ASSERT(parent != INVALID_FOLDER);
		FolderID folderID = folderPool.Alloc();
		ASSERT(folderID != INVALID_FOLDER);

		Folder& folder = GetFolder(folderID);
		CopyString(folder.name, "Folder");
		folder.parentFolder = parent;

		Folder& parentFolder = GetFolder(parent);
		if (parentFolder.childFolder != INVALID_FOLDER)
		{
			folder.nextFolder = parentFolder.childFolder;
			GetFolder(parentFolder.childFolder).prevFolder = folderID;
		}
		parentFolder.childFolder = folderID;

		return folderID;
	}

	void EntityFolder::DestroyFolder(FolderID folderID)
	{
		Folder& folder = GetFolder(folderID);
		Folder& parent = GetFolder(folder.parentFolder);
		if (parent.childFolder == folderID)
			parent.childFolder = folder.nextFolder;

		if (folder.nextFolder != INVALID_FOLDER)
			GetFolder(folder.nextFolder).prevFolder = folder.prevFolder;

		if (folder.prevFolder != INVALID_FOLDER)
			GetFolder(folder.prevFolder).nextFolder = folder.nextFolder;

		folderPool.Free(folderID);
		if (selectedFolder == folderID)
			folderID = 0;
	}

	EntityFolder::FolderID EntityFolder::GetFolder(ECS::Entity e)
	{
		auto it = entities.find(e);
		if (!it.isValid())
			return EntityFolder::INVALID_FOLDER;

		return it.value().folder;
	}

	EntityFolder::Folder& EntityFolder::GetFolder(FolderID folderID)
	{
		return folderPool.Get(folderID);
	}

	const EntityFolder::Folder& EntityFolder::GetFolder(FolderID folderID) const
	{
		return folderPool.Get(folderID);
	}

	EntityFolder::FolderID EntityFolder::FreeList::Alloc()
	{
		if (firstFree < 0)
		{
			data.emplace();
			return FolderID(data.size() - 1);
		}

		const FolderID id = (FolderID)firstFree;
		memcpy(&firstFree, &data[firstFree], sizeof(firstFree));
		new (&data[id]) Folder();
		return id;
	}

	void EntityFolder::FreeList::Free(FolderID folder)
	{
		memcpy(&data[folder], &firstFree, sizeof(firstFree));
		firstFree = folder;
	}
}

#endif