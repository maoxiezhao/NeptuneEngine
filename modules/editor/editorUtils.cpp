#include "editorUtils.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor::Utils
{
    Action::Action()
    {
        shortcut = Platform::Keycode::INVALID;
    }

    void Action::Init(const char* label_, const char* name_)
    {
        label = label_;
        name = name_;
        isSelected.Bind<FalseConst>();
        shortcut = Platform::Keycode::INVALID;
    }

    void Action::Init(const char* label_, const char* name_, const char* tooltip_, const char* icon_)
    {
        label = label_;
        name = name_;
        tooltip = tooltip_;
        icon = icon_;
        isSelected.Bind<FalseConst>();
        shortcut = Platform::Keycode::INVALID;
    }

    bool Action::IsActive()
    {
        if (ImGui::IsAnyItemFocused()) 
            return false;

        if (shortcut == Platform::Keycode::INVALID) 
            return false;

        if (shortcut != Platform::Keycode::INVALID && !Platform::IsKeyDown(shortcut))
            return false;

        return true;
    }

    bool Action::ToolbarButton(ImFont* font)
    {
        if (!icon[0]) 
            return false;

        const ImVec4 activeColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        const ImVec4 bgColor = isSelected.Invoke() ?  ImGui::GetStyle().Colors[ImGuiCol_Text] : activeColor;

        ImGui::SameLine();

        if (ImGuiEx::ToolbarButton(font, icon, bgColor, tooltip))
        {
            func.Invoke();
            return true;
        }
        return false;
    }
}
}