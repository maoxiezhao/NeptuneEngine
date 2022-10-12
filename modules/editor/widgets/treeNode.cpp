#include "treeNode.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	ContentTreeNode::ContentTreeNode(ContentFolderType type_, const Path& path_, ContentTreeNode* parent_) :
		type(type_),
		path(path_),
		parent(parent_)
	{
		name = Path::GetBaseName(path);
	}

	void ContentTreeNode::LoadFolder(bool checkSubDirs)
	{
		auto fileInfos = FileSystem::Enumerate(path);
		for (const auto& fileInfo : fileInfos)
		{
			if (fileInfo.filename[0] == '.')
				continue;

			if (fileInfo.type != PathType::Directory)
				continue;

			auto pos = children.find([&](const ContentTreeNode& node) {
				return node.path == fileInfo.filename;
			});
			if (pos < 0)
			{
				auto& node = children.emplace(type, path / fileInfo.filename, this);
				node.LoadFolder(true);
			}
			else if (checkSubDirs)
			{
				children[pos].LoadFolder(true);
			}
		}
	}

	MainContentTreeNode::MainContentTreeNode(ContentFolderType type_, const Path& path_, ContentTreeNode* parent_) :
		ContentTreeNode(type_, path_, parent_)
	{
		watcher = Platform::FileSystemWatcher::Create(path_);
		watcher->GetCallback().Bind<&MainContentTreeNode::OnFileWatcherEvent>(this);
	}

	void MainContentTreeNode::OnFileWatcherEvent(const Path& path, Platform::FileWatcherAction action)
	{
		if (action == Platform::FileWatcherAction::Create ||
			action == Platform::FileWatcherAction::Delete)
		{
		}
	}

	ProjectTreeNode::ProjectTreeNode(const ProjectInfo* projectInfo_) :
		ContentTreeNode(ContentFolderType::Other, projectInfo_->ProjectFolderPath, nullptr),
		projectInfo(projectInfo_)
	{
		name = projectInfo->Name;
	}

	ProjectTreeNode::~ProjectTreeNode()
	{
		CJING_SAFE_DELETE(contentNode);
		CJING_SAFE_DELETE(sourceNode);
	}
}
}