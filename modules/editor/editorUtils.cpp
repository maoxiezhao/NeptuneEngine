#include "editorUtils.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor::Utils
{
    Action::Action(const char* label_, const char* name_)
    {
        label = label_;
        name = name_;
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
}
}