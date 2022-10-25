#include "entityList.h"
#include "editor\editor.h"
#include "editor\modules\level.h"
#include "editor\modules\sceneEditing.h"
#include "imgui-docking\imgui.h"
#include "imgui-docking\imgui_internal.h"

namespace VulkanTest
{
namespace Editor
{
	EntityListWidget::EntityListWidget(EditorApp& editor_) :
		editor(editor_)
	{
		memset(folderRenameBuf, 0, sizeof(folderRenameBuf));
		memset(componentFilter, 0, sizeof(componentFilter));
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
			World* world = editor.GetSceneEditingModule().GetEditingWorld();
			if (world == nullptr)
			{
				ImGui::Text("No scene");
				ImGui::Text("Open from the content browser");
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
					EntityFolder& folders = editor.GetSceneEditingModule().GetEditingScene()->GetFolders();
					OnFolderUI(folders, folders.GetRoot(), -1, true);
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

		// Setup tree node flags
		ImGuiTreeNodeFlags flags = isRoot ? ImGuiTreeNodeFlags_DefaultOpen : 0;
		flags |= ImGuiTreeNodeFlags_OpenOnArrow;
		if (folders.GetSelectedFolder() == folderID) flags |= ImGuiTreeNodeFlags_Selected;

		bool nodeOpen = false;
		if (renamingFolder == folderID)
		{
			nodeOpen = ImGui::TreeNodeEx((void*)&folder, flags, "%s", isRoot ? "" : ICON_FA_FOLDER);
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
			nodeOpen = ImGui::TreeNodeEx((void*)&folder, flags, "%s%s", isRoot ? "" : ICON_FA_FOLDER, folder.name);
		}

		// Mosue drag enttiy to folder
		if (ImGui::BeginDragDropTarget())
		{
			auto* payload = ImGui::AcceptDragDropPayload("entity");
			if (payload != nullptr)
			{
				ECS::Entity droppedEntity = *(ECS::Entity*)payload->Data;
				droppedEntity.RemoveParent();
				folders.MoveToFolder(droppedEntity, folderID);
			}
			ImGui::EndDragDropTarget();
		}

		// Mouse select folder
		if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
			folders.SelectFolder(folderID);

		if (!nodeOpen)
		{
			ImGui::PopID();
			return;
		}
		
		auto& sceneEditing = editor.GetSceneEditingModule();
		auto ShowEntityMenu = [&]() {
			auto& selectedEntities = sceneEditing.GetSelectedEntities();
			bool entitySelected = !selectedEntities.empty();

			if (ImGui::BeginMenu(ICON_FA_PLUS_SQUARE "CreateEntity"))
			{
				ShowCreateEntityGUI(folderID);
				ImGui::EndMenu();
			}

			if (ImGui::Selectable(ICON_FA_MINUS_SQUARE "DeleteEntity", false, entitySelected ? 0 : ImGuiSelectableFlags_Disabled))
			{
				sceneEditing.DeleteEntity(selectedEntities[0]);
				sceneEditing.ClearSelectEntities();
			}	
		};

		if (isRoot && !ImGui::IsItemHovered() && ImGui::BeginPopupContextWindow("context"))
		{
			ShowEntityMenu();

			ImGui::Separator();

			if (ImGui::Selectable("New folder"))
			{
				//EntityFolder::FolderID newFolder = CreateFolder(folderID);
				//renamingFolder = newFolder;
				//folderRenameFocus = true;
				toCreateFolders.push_back(folderID);
			}
			ImGui::EndPopup();
		}

		// Folder context menu
		if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered())
			ImGui::OpenPopup("folderContextMenu");
		if (ImGui::BeginPopup("folderContextMenu"))
		{
			ShowEntityMenu();

			ImGui::Separator();

			if (ImGui::Selectable("New folder"))
			{
				//EntityFolder::FolderID newFolder = CreateFolder(folderID);
				//renamingFolder = newFolder;
				//folderRenameFocus = true;
				toCreateFolders.push_back(folderID);
			}

			if (!isRoot && depth > 0 && ImGui::Selectable("Rename")) 
			{
				renamingFolder = folderID;
				folderRenameFocus = true;
			}
			ImGui::EndPopup();
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
				ShowHierarchy(child, editor.GetSceneEditingModule().GetSelectedEntities());
			child = folders.GetNextEntity(child);
		}

		ImGui::TreePop();
		ImGui::PopID();
	}

	void EntityListWidget::ShowHierarchy(ECS::Entity entity, const Array<ECS::Entity>& selectedEntities)
	{
		auto& sceneEditing = editor.GetSceneEditingModule();

		Array<ECS::Entity> children;
		World* world = sceneEditing.GetEditingWorld();
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
			// Drag drop target
			if (ImGui::BeginDragDropTarget())
			{
				auto* payload = ImGui::AcceptDragDropPayload("entity");
				if (payload != nullptr)
				{
					ECS::Entity droppedEntity = *(ECS::Entity*)payload->Data;
					if (droppedEntity != entity)
					{
						sceneEditing.MakeParent(entity, droppedEntity);
						ImGui::EndDragDropTarget();
						if (nodeOpen)
							ImGui::TreePop();

						return;
					}
				}
			}

			// Drag drop srouce
			if (ImGui::BeginDragDropSource())
			{
				// Drag source entity
				ImGui::SetDragDropPayload("entity", &entity, sizeof(entity));
				ImGui::EndDragDropSource();
			}
			else
			{
				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
					sceneEditing.SelectEntities(Span(&entity, 1), ImGui::GetIO().KeyCtrl);
			}
		}

		if (nodeOpen)
		{
			for (const auto& child : children)
				ShowHierarchy(child, selectedEntities);
			ImGui::TreePop();
		}
	}

	const char* stristr(const char* haystack, const char* needle)
	{
		const char* c = haystack;
		while (*c)
		{
			if (MakeLowercase(*c) == MakeLowercase(needle[0]))
			{
				const char* n = needle + 1;
				const char* c2 = c + 1;
				while (*n && *c2)
				{
					if (MakeLowercase(*n) != MakeLowercase(*c2)) break;
					++n;
					++c2;
				}
				if (*n == 0) return c;
			}
			++c;
		}
		return nullptr;
	}

	static void ShowAddComonentTreeNode(const AddComponentTreeNode* node, const char* filter, EditorApp& editor)
	{
		if (node == nullptr)
			return;

		// Match filter
		if (filter[0] != 0)
		{
			if (!node->plugin)
				ShowAddComonentTreeNode(node->child, filter, editor);
			else if (stristr(node->plugin->GetLabel(), filter))
				node->plugin->OnGUI(false, true, editor);

			ShowAddComonentTreeNode(node->next, filter, editor);
			return;
		}

		if (node->plugin)
		{
			node->plugin->OnGUI(true, false, editor);
			ShowAddComonentTreeNode(node->next, filter, editor);
			return;
		}

		int slashPos = ReverseFindChar(node->label, '/');
		if (slashPos >= 0)
		{
			if (ImGui::BeginMenu(node->label + slashPos + 1))
			{
				ShowAddComonentTreeNode(node->child, filter, editor);
				ImGui::EndMenu();
			}
		}
		ShowAddComonentTreeNode(node->next, filter, editor);
	}

	void EntityListWidget::ShowCreateEntityGUI(EntityFolder::FolderID folderID)
	{
		auto& sceneEditing = editor.GetSceneEditingModule();
		if (sceneEditing.GetEditingWorld() == nullptr)
			return;

		if (ImGui::MenuItem("CreateEmpty"))
		{
			auto& folders = sceneEditing.GetEditingScene()->GetFolders();
			folders.SelectFolder(folderID);

			ECS::Entity entity = sceneEditing.AddEmptyEntity();
			sceneEditing.SelectEntities(Span(&entity, 1), false);
		}

		ImGui::Separator();

		const float w = ImGui::CalcTextSize(ICON_FA_TIMES).x + ImGui::GetStyle().ItemSpacing.x * 2;
		ImGui::SetNextItemWidth(-w);
		ImGui::InputTextWithHint("##filter", "Filter", componentFilter, sizeof(componentFilter));
		ImGui::SameLine();
		if (ImGuiEx::IconButton(ICON_FA_TIMES, "Clear filter")) {
			componentFilter[0] = '\0';
		}
		ShowAddComonentTreeNode(editor.GetAddComponentTreeNodeRoot()->child, componentFilter, editor);
	}

	const char* EntityListWidget::GetName()
	{
		return "EntityListWidget";
	}

	EntityFolder::FolderID EntityListWidget::CreateFolder(EntityFolder::FolderID folderID)
	{
		auto& sceneEditing = editor.GetSceneEditingModule();
		if (sceneEditing.GetEditingScene() == nullptr)
			return EntityFolder::INVALID_FOLDER;

		EntityFolder& folders = sceneEditing.GetEditingScene()->GetFolders();
		return folders.EmplaceFolder(folderID);
	}

	void EntityListWidget::RenameEntityFolder(EntityFolder::FolderID folderID, const char* name)
	{
		auto& sceneEditing = editor.GetSceneEditingModule();
		if (sceneEditing.GetEditingScene() == nullptr)
			return;

		EntityFolder& folders = sceneEditing.GetEditingScene()->GetFolders();
		EntityFolder::Folder& folder = folders.GetFolder(folderID);
		CopyString(folder.name, name);
	}
}
}