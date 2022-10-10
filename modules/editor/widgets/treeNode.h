#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    struct TreeNode
    {

    };

    enum class ContentFolderType
    {
        Content,
        Source,
        Other
    };

    struct MainContentTreeNode
    {
    public:
        ContentFolderType type;
        Path path;

    public:
        MainContentTreeNode(ContentFolderType type_, const Path& path_) :
            type(type_),
            path(path_)
        {
        }
    };

    struct ProjectTreeNode
    {

    };

}
}