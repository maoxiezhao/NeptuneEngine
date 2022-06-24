#include "entityList.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	EntityListWidget::EntityListWidget(EditorApp& editor_) :
		editor(editor_)
	{
	}

	EntityListWidget::~EntityListWidget()
	{
	}

	void EntityListWidget::Update(F32 dt)
	{
	}

	void EntityListWidget::OnGUI()
	{
		if (!isOpen) return;

		if (ImGui::Begin("Hierarchy##hierarchy", &isOpen))
		{
		}
		ImGui::End();
	}

	const char* EntityListWidget::GetName()
	{
		return "EntityListWidget";
	}
}
}


