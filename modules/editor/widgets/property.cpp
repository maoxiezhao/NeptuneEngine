#include "property.h"
#include "editor\editor.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	PropertyWidget::PropertyWidget(EditorApp& editor_) :
		editor(editor_)
	{
	}

	PropertyWidget::~PropertyWidget()
	{
	}

	void PropertyWidget::Update(F32 dt)
	{
	}

	void PropertyWidget::OnGUI()
	{
		if (!isOpen) return;

		if (ImGui::Begin("Inspector##inspector", &isOpen))
		{
		}
		ImGui::End();
	}

	const char* PropertyWidget::GetName()
	{
		return "PropertyWidget";
	}
}
}
