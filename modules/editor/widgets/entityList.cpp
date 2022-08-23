#include "entityList.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"
#include "imgui-docking\imgui_internal.h"

namespace VulkanTest
{
namespace Editor
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

			if (item.prev != ECS::INVALID_ENTITY)
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

	EntityListWidget::EntityListWidget(EditorApp& editor_) :
		editor(editor_),
		worldEditor(editor_.GetWorldEditor())
	{
		memset(folderRenameBuf, 0, sizeof(folderRenameBuf));
	}

	EntityListWidget::~EntityListWidget()
	{
	}

	void EntityListWidget::Update(F32 dt)
	{
		for (const auto& folderID : toCreateFolders)
		{
			EntityFolder::FolderID newFolder = CreateFolder(folderID);
			renamingFolder = newFolder;
			folderRenameFocus = true;
		}
		toCreateFolders.clear();
	}

	void EntityListWidget::OnGUI()
	{
		if (!isOpen) return;

		static char filter[64] = "";
		if (ImGui::Begin(ICON_FA_STREAM "Hierarchy##hierarchy", &isOpen))
		{
			World* world = worldEditor.GetWorld();
			if (world == nullptr)
			{
				ImGui::End();
				return;
			}

			// Show filter
			const float w = ImGui::CalcTextSize(ICON_FA_TIMES).x + ImGui::GetStyle().ItemSpacing.x * 2;
			ImGui::SetNextItemWidth(-w);
			ImGui::InputTextWithHint("##filter", "Filter", filter, sizeof(filter));
			ImGui::SameLine();
			if (ImGuiEx::IconButton(ICON_FA_TIMES, "Clear filter"))
				filter[0] = '\0';

			// Show entities
			if (ImGui::BeginChild("entities"))
			{
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x);

				if (filter[0] == '\0')
				{
					EntityFolder& folder = worldEditor.GetEntityFolder();
					OnFolderUI(folder, folder.GetRoot(), -1, true);
				}
			}
			ImGui::EndChild();

		}
		ImGui::End();
	}

	void EntityListWidget::OnFolderUI(EntityFolder& folders, EntityFolder::FolderID folderID, U32 depth, bool isRoot)
	{
		const EntityFolder::Folder& folder = folders.GetFolder(folderID);
		ImGui::PushID(&folder);

		if (!isRoot)
		{
			// Setup tree node flags
			ImGuiTreeNodeFlags flags = depth == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
			flags |= ImGuiTreeNodeFlags_OpenOnArrow;
			if (folders.GetSelectedFolder() == folderID) flags |= ImGuiTreeNodeFlags_Selected;

			bool nodeOpen = false;
			if (renamingFolder == folderID)
			{
				nodeOpen = ImGui::TreeNodeEx((void*)&folder, flags, "%s", ICON_FA_FOLDER);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
				ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
				if (folderRenameFocus) 
				{
					ImGui::SetKeyboardFocusHere();
					folderRenameFocus = false;
				}

				if (ImGui::InputText("##renamed_val", folderRenameBuf, sizeof(folderRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					RenameEntityFolder(renamingFolder, folderRenameBuf);
					memset(folderRenameBuf, 0, sizeof(folderRenameBuf));
				}
			
				if (ImGui::IsItemDeactivated())
					renamingFolder = EntityFolder::INVALID_FOLDER;
		
				folderRenameFocus = false;
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();
			}
			else
			{
				nodeOpen = ImGui::TreeNodeEx((void*)&folder, flags, "%s%s", ICON_FA_FOLDER, folder.name);
			}

			if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
				folders.SelectFolder(folderID);

			if (!nodeOpen)
			{
				ImGui::PopID();
				return;
			}
		}

		// Folder context menu
		if (isRoot)
		{
			if (!ImGui::IsItemHovered() && ImGui::BeginPopupContextWindow("context"))
			{
				if (ImGui::Selectable("New folder"))
				{
					//EntityFolder::FolderID newFolder = CreateFolder(folderID);
					//renamingFolder = newFolder;
					//folderRenameFocus = true;
					toCreateFolders.push_back(folderID);
				}
				ImGui::EndPopup();
			}
		}
		else
		{
			if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered())
				ImGui::OpenPopup("folderContextMenu");
			if (ImGui::BeginPopup("folderContextMenu"))
			{
				if (ImGui::Selectable("New folder"))
				{
					//EntityFolder::FolderID newFolder = CreateFolder(folderID);
					//renamingFolder = newFolder;
					//folderRenameFocus = true;
					toCreateFolders.push_back(folderID);
				}

				if (depth > 0 && ImGui::Selectable("Rename")) 
				{
					renamingFolder = folderID;
					folderRenameFocus = true;
				}
				ImGui::EndPopup();
			}
		}

		// Show folder children
		EntityFolder::FolderID childID = folder.childFolder;
		while (childID != EntityFolder::INVALID_FOLDER)
		{
			const auto& childFolder = folders.GetFolder(childID);
			OnFolderUI(folders, childID, depth + 1);
			childID = childFolder.nextFolder;
		}

		// Show entities
		ECS::Entity child = folder.firstEntity;
		while (child != ECS::INVALID_ENTITY)
		{
			if (child.GetParent() == ECS::INVALID_ENTITY)
				ShowHierarchy(child, worldEditor.GetSelectedEntities());
			child = folders.GetNextEntity(child);
		}

		if (!isRoot)
			ImGui::TreePop();

		ImGui::PopID();
	}

	void EntityListWidget::ShowHierarchy(ECS::Entity entity, const Array<ECS::Entity>& selectedEntities)
	{
		Array<ECS::Entity> children;
		World* world = worldEditor.GetWorld();
		world->EachChildren(entity, [&children](ECS::Entity child) {
			children.push_back(child);
		});

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;
		bool hasChild = children.size() > 0;
		if (!hasChild)
			flags = ImGuiTreeNodeFlags_Leaf;

		bool selected = selectedEntities.indexOf(entity) >= 0;
		if (selected)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool nodeOpen;
		const ImVec2 cp = ImGui::GetCursorPos();
		ImGui::Dummy(ImVec2(1.f, ImGui::GetTextLineHeightWithSpacing()));
		if (ImGui::IsItemVisible()) 
		{
			ImGui::SetCursorPos(cp);
			nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", entity.GetName());
		}
		else 
		{
			const char* dummy = "";
			const ImGuiID id = ImGui::GetCurrentWindow()->GetID((void*)(intptr_t)entity);
			if (ImGui::TreeNodeBehaviorIsOpen(id, flags)) 
			{
				ImGui::SetCursorPos(cp);
				nodeOpen = ImGui::TreeNodeBehavior(id, flags, dummy, dummy);
			}
			else 
			{
				nodeOpen = false;
			}
		}

		if (ImGui::IsItemVisible())
		{
			if (!ImGui::BeginDragDropSource())
			{
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					worldEditor.SelectEntities(Span(&entity, 1), ImGui::GetIO().KeyCtrl);
			}
		}

		if (nodeOpen)
		{
			for (const auto& child : children)
				ShowHierarchy(child, selectedEntities);
			ImGui::TreePop();
		}
	}

	const char* EntityListWidget::GetName()
	{
		return "EntityListWidget";
	}

	EntityFolder::FolderID EntityListWidget::CreateFolder(EntityFolder::FolderID folderID)
	{
		// TODO: use EditorCommand
		EntityFolder& folder = worldEditor.GetEntityFolder();
		return folder.EmplaceFolder(folderID);
	}

	void EntityListWidget::RenameEntityFolder(EntityFolder::FolderID folderID, const char* name)
	{
		// TODO: use EditorCommand
		EntityFolder::Folder& folder = worldEditor.GetEntityFolder().GetFolder(folderID);
		CopyString(folder.name, name);
	}
}
}