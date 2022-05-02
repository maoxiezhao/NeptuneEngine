#include "editorUtils.h"

namespace VulkanTest
{
namespace Editor::Utils
{
    Action::Action(const char* label_, const char* name_)
    {
        label = label_;
        name = name_;
        isSelected.Bind<FalseConst>();
    }
}
}