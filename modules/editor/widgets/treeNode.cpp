#include "treeNode.h"
#include "editor\editor.h"
#include "editor\widgets\assetBrowser.h"

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

	ContentTreeNode::~ContentTreeNode()
	{
		children.clearDelete();
	}

	void ContentTreeNode::LoadFolder(bool checkSubDirs)
	{
		for (int i = 0; i < children.size(); i++)
		{
			auto child = children[i];
			if (!Platform::DirExists(child->path))
			{
				CJING_DELETE(child);
				children.swapAndPop(i);
				i--;
			}
		}

		auto fileInfos = FileSystem::Enumerate(path);
		for (const auto& fileInfo : fileInfos)
		{
			if (fileInfo.filename[0] == '.')
				continue;

			if (fileInfo.type != PathType::Directory)
				continue;

			auto fullPath = path / fileInfo.filename;
			auto pos = children.find([&](ContentTreeNode* node) {
				return node->path == fullPath;
			});
			if (pos < 0)
			{
				auto node = CJING_NEW(ContentTreeNode)(type, fullPath, this);
				node->LoadFolder(true);
				children.push_back(node);
			}
			else if (checkSubDirs)
			{
				children[pos]->LoadFolder(true);
			}
		}
	}

	void ContentTreeNode::RefreshFolder(const Path& targetPath, bool checkSubDirs)
	{
		// TODO
		LoadFolder(checkSubDirs);
	}

	MainContentTreeNode::MainContentTreeNode(EditorApp& editor_, ContentFolderType type_, const Path& path_, ContentTreeNode* parent_) :
		ContentTreeNode(type_, path_, parent_),
		editor(editor_)
	{
		watcher = Platform::FileSystemWatcher::Create(path_);
		watcher->GetCallback().Bind<&MainContentTreeNode::OnFileWatcherEvent>(this);
	}

	void MainContentTreeNode::OnFileWatcherEvent(const Path& path, Platform::FileWatcherAction action)
	{
		if (action == Platform::FileWatcherAction::Create ||
			action == Platform::FileWatcherAction::Delete)
		{
			editor.GetAssetBrowser().OnDirectoryEvent(this, path, action);
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
		children.clear();
		CJING_SAFE_DELETE(contentNode);
		CJING_SAFE_DELETE(sourceNode);
	}

	void ProjectTreeNode::LoadFolder(bool checkSubDirs)
	{
		if (contentNode != nullptr)
		{
			contentNode->LoadFolder(checkSubDirs);
			children.push_back(contentNode);
		}
	}
}
}