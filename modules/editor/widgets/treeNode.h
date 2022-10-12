#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "editor\projectInfo.h"
#include "core\platform\platform.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    enum class ContentFolderType
    {
        Content,
        Source,
        Other
    };

    struct ContentTreeNode
    {
        ContentFolderType type = ContentFolderType::Other;
        Path path;
        String name;
        ContentTreeNode* parent = nullptr;
        Array<ContentTreeNode> children;

    public:
        ContentTreeNode(ContentFolderType type_, const Path& path_, ContentTreeNode* parent_);
        void LoadFolder(bool checkSubDirs);
    };

    struct MainContentTreeNode : public ContentTreeNode
    {
    private:
        UniquePtr<Platform::FileSystemWatcher> watcher;

    public:
        MainContentTreeNode(ContentFolderType type_, const Path& path_, ContentTreeNode* parent_);

        void OnFileWatcherEvent(const Path& path, Platform::FileWatcherAction action);
    };

    struct ProjectTreeNode : ContentTreeNode
    {
        const ProjectInfo* projectInfo;
        MainContentTreeNode* sourceNode = nullptr;
        MainContentTreeNode* contentNode = nullptr;

    public:
        ProjectTreeNode(const ProjectInfo* projectInfo_);
        ~ProjectTreeNode();
    };

}
}