#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
	struct PropertyEditor
	{
	private:
		static void OnAttributeImpl(const char* name, bool isReadonly, std::function<void()> func)
		{
			if (isReadonly)
				ImGuiEx::PushReadonly();

			ImGuiEx::Label(name);
			ImGui::PushID(name);
			func();
			ImGui::PopID();

			if (isReadonly)
				ImGuiEx::PopReadonly();
		}

	public:
		static void OnAttribute(const char* name, F32& value, bool isReadonly = false)
		{
			OnAttributeImpl(name, isReadonly, [&]() {
				ImGui::DragFloat("##v", &value, 1, -100, 100);
			});
		}

		static void OnAttribute(const char* name, bool& value, bool isReadonly = false)
		{
			OnAttributeImpl(name, isReadonly, [&]() {
				ImGui::Checkbox("##v", &value);
			});
		}
	};

#define ATTRIBUTE_EDITOR(name) PropertyEditor::OnAttribute(#name, settings.name);
}
}
