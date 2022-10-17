#include "assetBrowser.h"
#include "assetImporter.h"
#include "treeNode.h"
#include "assetItem.h"
#include "editor\editor.h"
#include "editor\widgets\codeEditor.h"
#include "editor\modules\thumbnails.h"
#include "core\platform\platform.h"
#include "content\resources\texture.h"
#include "renderer\render2D\fontResource.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{   
    I32 AssetBrowser::TileSize = 96;
    F32 AssetBrowser::ThumbnailSize = 1.0f;

    class AssetBrowserImpl : public AssetBrowser
    {
    public:
        EditorApp& editor;
        bool initialized = false;
        float leftColumnWidth = 120;
        bool showThumbnails = true;

        I32 popupCtxItem = -1;
        char filter[128];
        ContentTreeNode* curNode = nullptr;

        Array<AssetItem*> contentItems;  // All file infos in the current directory
        Array<AssetItem*> selectedItems;
        bool hasResInClipboard = false;

        CodeEditor codeEditor;
        HashMap<U64, IPlugin*> plugins;

        I32 historyIndex = -1;
        Array<Path> history;
        Utils::Action backAction;
        Utils::Action forwardAction;
        Utils::Action importAction;

        ProjectTreeNode projectTreeNode;
        ProjectTreeNode engineTreeNode;

    public:
        AssetBrowserImpl(EditorApp& editor_) :
            editor(editor_),
            codeEditor(editor_),
            projectTreeNode(ProjectInfo::EditorProject),
            engineTreeNode(ProjectInfo::EngineProject)
        {
            filter[0] = '\0';

#if 0
            // Check directory of resource tiles
            StaticString<MAX_PATH_LENGTH> tilePath(".export/resources_tiles");
            if (!Platform::DirExists(tilePath.c_str()))
                Platform::MakeDir(tilePath.c_str());
#endif

            backAction.Init("Move back", "back", "Back in asset history", ICON_FA_ARROW_LEFT);
            backAction.func.Bind<&AssetBrowserImpl::GoBack>(this);

            forwardAction.Init("Move forward", "forward", "Forward in asset history", ICON_FA_ARROW_RIGHT);
            forwardAction.func.Bind<&AssetBrowserImpl::GoForward>(this);

            importAction.Init("Import", "import", "Import resource", ICON_FA_ARROW_DOWN);
            importAction.func.Bind<&AssetBrowserImpl::Import>(this);

            engineTreeNode.contentNode = CJING_NEW(MainContentTreeNode)(ContentFolderType::Content, Globals::EngineContentFolder, &engineTreeNode);
            engineTreeNode.contentNode->LoadFolder(true);
        }

        void GoBack()
        {
            if (historyIndex < 1) 
                return;
            historyIndex = std::max(0, historyIndex - 1);
        }

        void Import()
        {
            if (curNode == nullptr)
                return;

            editor.GetAssetImporter().ShowImportFileDialog(curNode->path);
        }

        void GoForward()
        {
            historyIndex = std::min(historyIndex + 1, (I32)history.size() - 1);
        }

        ~AssetBrowserImpl()
        {
            contentItems.clearDelete();
        }

        void InitFinished() override
        {
            initialized = true;
        }

        void Update(F32 dt) override
        { 
            PROFILE_FUNCTION();
            for (auto kvp : plugins) 
                kvp->Update();
        }

        void OnGUI() override
        {
            // Set default directory
            if (curNode == nullptr)
                SetCurrentTreeNode(projectTreeNode.contentNode);

            if (!isOpen)
            {
                OnDetailsGUI();
                return;
            }
            
            if (!ImGui::Begin(ICON_FA_IMAGES "Assets##assets", &isOpen)) {
                ImGui::End();
                OnDetailsGUI();
                return;
            }

#if 0
            // Show serach field
            ImGui::PushItemWidth(150);
            if (ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Search", filter, sizeof(filter), ImGuiInputTextFlags_EnterReturnsTrue))
                SetCurrentDir(curDir);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGuiEx::IconButton(ICON_FA_TIMES, "Clear search")) 
            {
                filter[0] = '\0';
                SetCurrentDir(curDir);
            }
#endif
            // Show tool bar
            OnToolbarGUI();
            ImGui::Separator();

            float contentW = ImGui::GetContentRegionAvail().x;
            ImVec2 leftSize(leftColumnWidth, 0);
            if (leftSize.x < 10) leftSize.x = 10;
            if (leftSize.x > contentW - 10) leftSize.x = contentW - 10;

            // Show left dir column
            OnDirColumnGUI();

            ImGui::SameLine();
            ImGuiEx::VSplitter("vsplit1", &leftSize);
            if (leftSize.x >= 120)
                leftColumnWidth = leftSize.x;
            ImGui::SameLine();

            // Show right file column
            OnFileColumnGUI();

            ImGui::End();

            OnDetailsGUI();
        }

        const char* GetName() override
        {
            return "AssetBrowser";
        }

        void OpenInExternalEditor(Resource* resource) override
        {
            OpenInExternalEditor(resource->GetPath().c_str());
        }

        void OpenInExternalEditor(const char* path) override
        {
            codeEditor.OpenFile(path);
        }

        void AddPlugin(IPlugin& plugin) override
        {
            plugins.insert(plugin.GetResourceType().GetHashValue(), &plugin);
        }

        void RemovePlugin(IPlugin& plugin) override
        {
            plugins.erase(plugin.GetResourceType().GetHashValue());
        }

        void OnResourceListChanged(const Path& path)
        {
#if 0
            const StaticString<MAX_PATH_LENGTH> fullPath(path.c_str());
            if (Platform::DirExists(fullPath.c_str()))
            {
                SetCurrentDir(curDir);
                return;
            }

            Span<const char> dir = Path::GetDir(path.c_str());
            if (dir.length() > 0 && (*(dir.pEnd - 1) == '/' || *(dir.pEnd - 1) == '\\')) {
                --dir.pEnd;
            }
            if (!EqualString(dir, Span<const char>(curDir, curDir.length())))
                return;

            auto renderInterface = editor.GetRenderInterface();
            for (auto& info : contentItems)
            {
                if (info.filepath != path.c_str()) 
                    continue;

                switch (GetTileState(info))
                {
                case TileState::OK:
                    return;
                case TileState::OUTDATED:
                case TileState::NOT_CREATED:
                    if (info.tex != nullptr)
                    {
                        renderInterface->DestroyTexture(info.tex);
                        info.tex = nullptr;
                    }
                    info.isLoading = false;
                    break;
                case TileState::DELETED:
                    if (info.tex != nullptr)
                    {
                        renderInterface->DestroyTexture(info.tex);
                        info.tex = nullptr;
                    }
                    ResourceManager::DeleteResource(Path(path));
                    break;
                default:
                    break;
                }
            }

            contentItems.clear();
            AddAssetItem(path);

#endif
        }

        AssetItem* ConstructItem(const Path& path, ResourceInfo& resInfo)
        {
            const auto& resType = resInfo.type;
            if (resType == ResourceType::INVALID_TYPE)
                return nullptr;

            return CJING_NEW(ResourceItem)(path, resInfo.guid, ResourceType::GetResourceTypename(resType));
        }

        void LoadResoruces(ContentTreeNode* treeNode, std::vector<ListEntry>& fileList)
        {
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.filename[0] == '.' || fileInfo.type == PathType::Directory)
                    continue;

                if (!EndsWith(fileInfo.filename, RESOURCE_FILES_EXTENSION_WITH_DOT))
                    continue;

                AssetItem* item = nullptr;
                const Path resPath = treeNode->path / fileInfo.filename;
                ResourceInfo resInfo;
                if (ResourceManager::GetResourceInfo(resPath, resInfo))
                    item = ConstructItem(resPath, resInfo);
                if (item == nullptr)
                    item = CJING_NEW(FileItem)(resPath);

                contentItems.push_back(item);
            }
        }

        void RefreshCurrentTreeNode()
        {
            // Clear file infos
            auto renderInterface = editor.GetRenderInterface();
            for (auto& info : contentItems)
            {
                if (info->tex != nullptr)
                    renderInterface->DestroyTexture(info->tex);
            }
            contentItems.clearDelete();
            selectedItems.clear();

            if (curNode == nullptr)
                return;

            // Load directories
            Path curDir = curNode->path;
            auto fileList = FileSystem::Enumerate(curDir.c_str());
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.filename[0] == '.' || fileInfo.type != PathType::Directory)
                    continue;

                contentItems.push_back(CJING_NEW(ContentFolderItem)(Path(fileInfo.filename)));
            }

            // Load resources
            bool canHaveResources = true;
            if (canHaveResources)
                LoadResoruces(curNode, fileList);
        }

        void SetCurrentTreeNode(ContentTreeNode* treeNode)
        {
            if (treeNode == curNode)
                return;

            curNode = treeNode;

            RefreshCurrentTreeNode();

#if 0
            // Add the resource with the same directory
            RuntimeHash hash(curDir.c_str());
            auto& resources = compiler.LockResources();
            for (auto& res : resources)
            {
                if (res.dirHash == hash)
                    AddAssetItem(res.path);
            }
            compiler.UnlockResources();
#endif
        }

        void OnDetailsGUI()
        {
            if (!isOpen)
                return;

#if 0
            if (ImGui::Begin(ICON_FA_IMAGE "Asset inspector##asset_inspector", &isOpen, ImGuiWindowFlags_AlwaysVerticalScrollbar))
            {
                if (selectedResources.empty())
                {
                    ImGui::End();
                    return;
                }

                if (selectedResources.size() == 1)
                {
                    Resource* res = selectedResources[0].get();
                    const char* path = res->GetPath().c_str();
                    ImGuiEx::Label("Selected resource");
                    ImGui::TextUnformatted(path);
                    ImGui::Separator();

                    ImGuiEx::Label("Status");
                    ImGui::TextUnformatted(res->IsFailure() ? "failure" : (res->IsReady() ? "Ready" : "Not ready"));
                    ImGuiEx::Label("Compiled size");
                    if (res->IsReady()) {
                        ImGui::Text("%.2f KB", res->Size() / 1024.f);
                    }
                    else {
                        ImGui::TextUnformatted("N/A");
                    }

                    // Plugin does the own OnGUI
                    ResourceType resType = editor.GetAssetImporter().GetResourceType(path);
                    auto it = plugins.find(resType.GetHashValue());
                    if (it.isValid())
                    {
                        ImGui::Separator();
                        it.value()->OnGui(res);
                    }
                }
                else
                {
                    ImGui::Separator();
                    ImGuiEx::Label("Selected resource");
                    ImGui::TextUnformatted("multiple");
                    ImGui::Separator();
                }
            }
            ImGui::End();
#endif
        }

        void OnTreeNodeUI(ContentTreeNode* treeNode, bool isRoot)
        {
            if (treeNode == nullptr)
                return;

            ImGui::PushID(treeNode);

            // Setup tree node flags
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
            bool isLeaf = !isRoot && treeNode->children.empty();
            if (isLeaf)
                flags |= ImGuiTreeNodeFlags_Leaf;

            bool isSelected = curNode == treeNode;
            if (isSelected)
                flags |= ImGuiTreeNodeFlags_Selected;

            if (curNode != nullptr)
            {
                if (StartsWith(curNode->path.c_str(), treeNode->path.c_str()))
                    flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }

            bool nodeOpen = ImGui::TreeNodeEx((void*)&treeNode, flags, "%s%s", isRoot ? ICON_FA_HOME : ICON_FA_FOLDER, treeNode->name.c_str());
            if (ImGui::IsItemVisible())
            {      
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    SetCurrentTreeNode(treeNode);
            }

            if (!nodeOpen)
            {
                ImGui::PopID();
                return;
            }

            // Folder context menu
            if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered())
                ImGui::OpenPopup("directroyContextMenu");
            if (ImGui::BeginPopup("directroyContextMenu"))
            {
                if (ImGui::Selectable("Expand all"))
                {
                }

                if (ImGui::Selectable("Collapse all"))
                {
                }

                ImGui::Separator();

                ImGui::EndPopup();
            }

            // Show children folders
            if (isRoot)
            {
                ProjectTreeNode* projectNode = (ProjectTreeNode*)treeNode;
                if (projectNode->contentNode)
                    OnTreeNodeUI(projectNode->contentNode, false);
                if (projectNode->sourceNode)
                    OnTreeNodeUI(projectNode->sourceNode, false);
            }

            for (auto& child : treeNode->children)
            {
                OnTreeNodeUI(&child, false);
            }

            ImGui::TreePop();
            ImGui::PopID();
        }

        void OnDirColumnGUI()
        {
            ImVec2 size(std::max(160.f, leftColumnWidth), 0);
            ImGui::BeginChild("left_col", size);
            ImGui::PushItemWidth(160);

            OnTreeNodeUI(&projectTreeNode, true);
            OnTreeNodeUI(&engineTreeNode, true);
#if 0
            bool selected = false;
            // Show back selectable
            if ((curDir[0] != '.' || curDir[1] != 0) && ImGui::Selectable("..", &selected))
            {
                char dir[MAX_PATH_LENGTH];
                CopyString(Span(dir), Path::GetDir(curDir));
                SetCurrentDir(dir);
            }

            // Show subdirs
            for (auto& subdir : subdirs)
            {
                if (ImGui::Selectable(subdir, &selected))
                {
                    auto newDir = Path::Join(curDir.toSpan(), subdir.toSpan());
                    SetCurrentDir(newDir);
                }
            }
#endif
            ImGui::PopItemWidth();
            ImGui::EndChild();
        }

        void OnToolbarGUI()
        {
            auto pos = ImGui::GetCursorScreenPos();
            const float toolbarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2;
            if (ImGuiEx::BeginToolbar("browserToolbar", pos, ImVec2(0, toolbarHeight)))
            {
                auto iconFont = editor.GetBigIconFont();
                importAction.ToolbarButton(iconFont);
   
                bool histroyEnable = history.size() > 1;
                backAction.ToolbarButton(iconFont, histroyEnable && historyIndex > 0);
                forwardAction.ToolbarButton(iconFont, histroyEnable && historyIndex < history.size() - 1);

                ImGui::SameLine();
                ImGuiEx::Rect(1.0f, ImGui::GetItemRectSize().y, 0xff3A3A3E);
                ImGui::SameLine();

                // Show bread crumbs
                BreadCrumbs();
            }
            
            ImGuiEx::EndToolbar();
        }

        void BreadCrumbs()
        {
            if (curNode == nullptr)
            {
                ImGui::SameLine();
                ImGui::TextUnformatted("/");
                return;
            }

            // Show buttons for current directory
            static Array<ContentTreeNode*> breadQueue;
            auto node = curNode;
            while (node != nullptr)
            {
                breadQueue.push_back(node);
                node = node->parent;
            }

            for (int i = breadQueue.size() - 1; i >= 0; i--)
            {
                auto node = breadQueue[i];
                if (node == nullptr)
                    continue;

                if (ImGui::Button(node->name.c_str()))
                    SetCurrentTreeNode(node);
                ImGui::SameLine();
                ImGui::TextUnformatted("/");
                ImGui::SameLine();
            }
            breadQueue.clear();

            ImGui::NewLine();
        }

        void ShowCommonPopup()
        {
            if (curNode == nullptr)
                return;

            if (ImGui::MenuItem("Copy", nullptr, false, popupCtxItem >= 0))
            {
            }

            if (ImGui::MenuItem("Paste", nullptr, false, hasResInClipboard))
            {
            }

            static char tmp[MAX_PATH_LENGTH] = "";
            ImGui::Separator();
            ImGui::Checkbox("Thumbnails", &showThumbnails);

            if (ImGui::MenuItem("View in explorer")) 
            {
                 Platform::OpenExplorer(curNode->path);     
            }
            if (ImGui::BeginMenu("New folder")) 
            {
                ImGui::InputTextWithHint("##dirname", "New directory name", tmp, sizeof(tmp));
                ImGui::SameLine();
                if (ImGui::Button("Create")) 
                {
                    Path fullPath = curNode->path / tmp;
                    if (!Platform::MakeDir(fullPath))
                        Logger::Error("Failed to create directory %s", fullPath.c_str());

                    RefreshCurrentTreeNode();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create"))
            {
                for (auto plugin : plugins)
                {
                    if (!plugin->CreateResourceEnable())
                        continue;

                    if (ImGui::BeginMenu(plugin->GetResourceName()))
                    {
                        ImGui::InputTextWithHint("##name", "Name", tmp, sizeof(tmp));
                        ImGui::SameLine();
                        if (ImGui::Button("Create")) 
                        {
                            // plugin->CreateResource(Path(curDir), tmp);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndMenu();
                    }
                }
            ImGui::EndMenu();
            }

            //if (ImGui::MenuItem("Refresh"))
            //    SetCurrentDir(curDir);
        }

        void OpenResourceItem(AssetItem& tile)
        {
        }

        void SelectResourceItem(AssetItem& tile, bool modified)
        {
            if (!modified)
                selectedItems.clear();

            auto idx = selectedItems.indexOf(&tile);
            if (idx < 0)
                selectedItems.push_back(&tile);
        }

        void HandleResourceItem(AssetItem& tile, int idx)
        {
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tile.filepath.c_str());

            if (tile.type == AssetItemType::Directory)
            {
                if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        if (curNode && !curNode->children.empty())
                        {
                            for (auto& node : curNode->children)
                            {
                                if (node.name == tile.filename)
                                    SetCurrentTreeNode(&node);
                            }
                        }
                    }
                    else if (ImGui::IsMouseReleased(0))
                    {
                        SelectResourceItem(tile, ImGui::GetIO().KeyCtrl);
                    }
                }
            }
            else if (tile.type == AssetItemType::Resource)
            {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::Text("%s", (const char*)tile.filepath);
                    ImGui::SetDragDropPayload("ResPath", tile.filepath, StringLength(tile.filepath) + 1, ImGuiCond_Once);
                    ImGui::EndDragDropSource();
                }
                else if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseDoubleClicked(0))
                        OpenResourceItem(tile);
                    else if (ImGui::IsMouseReleased(0))
                        SelectResourceItem(tile, ImGui::GetIO().KeyCtrl);
                    else if (ImGui::IsMouseReleased(1))
                    {
                        popupCtxItem = idx;
                        ImGui::OpenPopup("itemCtx");
                    }
                }
            }   
        }

        void DeleteAssetItem(AssetItem* item)
        {
            if (item == nullptr || item->filepath.IsEmpty())
                return;

            FileSystem::DeleteFile(item->filepath.c_str());
            selectedItems.clear();
        }

        void OnFileColumnGUI()
        {
            ImGui::BeginChild("file_col");
            float w = ImGui::GetContentRegionAvail().x;
            int columns = std::max(1, showThumbnails ? (int)w / int(TileSize * ThumbnailSize) : 1);
            int tileCount = contentItems.size();
            int rows = showThumbnails ? (tileCount + columns - 1) / columns : tileCount;
            auto GetThumbnailIndex = [this](int i, int j, int columns) {
                int idx = j * columns + i;
                if (idx >= (int)contentItems.size()) {
                    return -1;
                }
                return idx;
            };

            ImGuiListClipper clipper;
            clipper.Begin(rows);
            while (clipper.Step())
            {
                for (int j = clipper.DisplayStart; j < clipper.DisplayEnd; ++j)
                {
                    if (showThumbnails)
                    {
                        for (int i = 0; i < columns; ++i)
                        {
                            if (i > 0) ImGui::SameLine();
                            int idx = GetThumbnailIndex(i, j, columns);
                            if (idx < 0) 
                            {
                                ImGui::NewLine();
                                break;
                            }

                            AssetItem& tile = *contentItems[idx];
                            bool selected = selectedItems.find([&](AssetItem* item) {
                                return item == &tile;
                            }) >= 0;           
                            ShowThumbnail(tile, TileSize * ThumbnailSize, selected);
                            HandleResourceItem(tile, idx);
                        }
                    }
                    else
                    {
                        AssetItem& tile = *contentItems[j];
                        bool selected = selectedItems.find([&](AssetItem* item) {
                            return item == &tile;
                        }) >= 0;
                        ImGui::Selectable(tile.filepath, selected);
                        HandleResourceItem(tile, j);
                    }
                }
            }

            // Popup menu
            bool openDeletePopup = false;
            if (ImGui::BeginPopup("itemCtx"))
            {
                ImGui::Text("%s", contentItems[popupCtxItem]->filename.data);
                ImGui::Separator();
                if (ImGui::MenuItem("Open"))
                {
                    // TODO Open a resource viewer
                }

                if (ImGui::MenuItem(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
                    OpenInExternalEditor(contentItems[popupCtxItem]->filepath);

                if (ImGui::BeginMenu("Rename"))
                {
                    static char tmp[MAX_PATH_LENGTH] = "";
                    ImGui::InputTextWithHint("##New name", "New name", tmp, sizeof(tmp));
                    if (ImGui::Button("Rename", ImVec2(100, 0)))
                    {
                        PathInfo pathInfo(contentItems[popupCtxItem]->filepath);
                        StaticString<MAX_PATH_LENGTH> newPath(pathInfo.dir, tmp, ".", pathInfo.extension);
                        FileSystem::MoveFile(contentItems[popupCtxItem]->filepath.c_str(), newPath.c_str());
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Delete"))
                    openDeletePopup = true;

                ImGui::Separator();
                ShowCommonPopup();
                ImGui::EndPopup();
            }
            else if (ImGui::BeginPopupContextWindow("context")) 
            {
                popupCtxItem = -1;
                ShowCommonPopup();
                ImGui::EndPopup();
            }

            // Open delete popup
            if (openDeletePopup) 
                ImGui::OpenPopup("Delete_file");
            if (ImGui::BeginPopupModal("Delete_file", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Are you sure to delete %s? This can not be undone.", contentItems[popupCtxItem]->filename.c_str());
                if (ImGui::Button("Yes", ImVec2(100, 0))) 
                {           
                    DeleteAssetItem(contentItems[popupCtxItem]);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 100 - ImGui::GetStyle().WindowPadding.x);
                if (ImGui::Button("Cancel", ImVec2(100, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::EndChild();
        }

        void ShowThumbnail(AssetItem& info, F32 size, bool selected)
        {
            ImGui::BeginGroup();
            ImVec2 imgSize(size, size);

            // Show thumbnail image
            if (info.tex)
            {
                ImGui::Image(info.tex, imgSize);
            }
            else
            {
                ImGuiEx::Rect(imgSize.x, imgSize.y, 0xffffFFFF);
                editor.GetThumbnailsModule().RefreshThumbnail(info);
            }

            // Show filename
            ImVec2 pos = ImGui::GetCursorPos();
            ImVec2 textSize = ImGui::CalcTextSize(info.filename.c_str());
            pos.x += (size - textSize.x) * 0.5f;
            ImGui::SetCursorPos(pos);
            ImGui::Text("%s", info.filename.c_str());
            ImGui::EndGroup();

            if (selected)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const U32 color = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
                dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color, 0, 0, 3.f);
            }
        }

#if 0
        void SelectResource(const Path& path, bool recordHistory)
        {
            if (recordHistory)
            {
                while (historyIndex < history.size() - 1)
                    history.pop_back();
      
                historyIndex++;
                history.push_back(path);
                if (history.size() > 20)
                {
                    --historyIndex;
                    history.eraseAt(0);
                }
            }

            const ResourceType resType = editor.GetAssetImporter().GetResourceType(path.c_str());
            ResPtr<Resource> res = ResourceManager::LoadResource(resType, path);
            if (res)
            {
                UnloadSelectedResources();
                selectedResources.push_back(std::move(res));
            }
        }

        void DoubleClickResource(const Path& path)
        {
            for (auto plugin : plugins)
                plugin->DoubleClick(path);
        }

        void UnloadSelectedResources()
        {
            if (selectedResources.empty())
                return;

            for (auto res : selectedResources)
            {
                for (auto plugin : plugins)
                    plugin->OnResourceUnloaded(res.get());
                res.reset();
            }

            selectedResources.clear();
        }
#endif
    };

    UniquePtr<AssetBrowser> AssetBrowser::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<AssetBrowserImpl>(app);
    }
}
}