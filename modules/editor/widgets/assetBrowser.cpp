#include "assetBrowser.h"
#include "assetImporter.h"
#include "treeNode.h"
#include "assetItem.h"
#include "editor\editor.h"
#include "editor\importers\resourceImportingManager.h"
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
    static constexpr int TILE_SIZE = 96;

    static void ClampText(char* text, int width)
    {
        char* end = text + StringLength(text);
        ImVec2 size = ImGui::CalcTextSize(text);
        if (size.x <= width) return;

        do
        {
            *(end - 1) = '\0';
            *(end - 2) = '.';
            *(end - 3) = '.';
            *(end - 4) = '.';
            --end;

            size = ImGui::CalcTextSize(text);
        } while (size.x > width && end - text > 4);
    }

    class AssetBrowserImpl : public AssetBrowser
    {
    public:
        EditorApp& editor;
        bool initialized = false;
        float leftColumnWidth = 120;
        bool showThumbnails = true;
        float thumbnailSize = 1.f;

        Array<ResPtr<Resource>> selectedResources;
        I32 popupCtxResource = -1;
        char filter[128];
        ContentTreeNode* curNode = nullptr;

      
        Array<AssetItem> fileInfos;  // All file infos in the current directory
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
            SelectResource(history[historyIndex], false);
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
            SelectResource(history[historyIndex], false);
        }

        ~AssetBrowserImpl()
        {
            UnloadSelectedResources();
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

            // Show serach field
#if 0
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
            for (auto& info : fileInfos)
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

            fileInfos.clear();
            AddAssetItem(path);

#endif
        }

        void SetCurrentTreeNode(ContentTreeNode* treeNode)
        {
            if (treeNode == curNode)
                return;

            curNode = treeNode;

            // Clear file infos
            auto renderInterface = editor.GetRenderInterface();
            for (auto& info : fileInfos)
            {
                if (info.tex != nullptr)
                    renderInterface->DestroyTexture(info.tex);
            }
            fileInfos.clear();
       
            if (curNode == nullptr)
                return;

            Path curDir = treeNode->path;
            auto fileList = FileSystem::Enumerate(curDir.c_str());
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.filename[0] == '.' || fileInfo.type != PathType::Directory)
                    continue;
                
                AddAssetItem(Path(fileInfo.filename), AssetItemType::Directory);
            }

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

        void AddAssetItem(const Path& path, AssetItemType type)
        {
            AssetItem info = {};
            info.filepath = path.c_str();
            info.type = type;

            char filename[MAX_PATH_LENGTH];
            CopyString(filename, Path::GetBaseName(path.c_str()));
            ClampText(filename, I32(thumbnailSize * TILE_SIZE));
            info.filename = filename;

            fileInfos.push_back(info);
        }

        void DeleteResTile(I32 index)
        {
            ResourceManager::DeleteResource(Path(fileInfos[index].filepath.c_str()));
            FileSystem::DeleteFile(fileInfos[index].filepath.c_str());
            selectedResources.clear();
        }

        void OnDetailsGUI()
        {
            if (!isOpen)
                return;

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

            bool nodeOpen = nodeOpen = ImGui::TreeNodeEx((void*)&treeNode, flags, "%s%s", isRoot ? ICON_FA_HOME : ICON_FA_FOLDER, treeNode->name.c_str());

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
   
                if (history.size() > 1) 
                {
                    if (historyIndex > 0)
                        backAction.ToolbarButton(iconFont);
                    if (historyIndex < history.size() - 1)
                        forwardAction.ToolbarButton(iconFont);
                }

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
            if (ImGui::MenuItem("Copy", nullptr, false, popupCtxResource >= 0))
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
                // StaticString<MAX_PATH_LENGTH> fullPath(curDir);
                // Platform::OpenExplorer(fullPath.c_str());     
            }
            if (ImGui::BeginMenu("New folder")) 
            {
                ImGui::InputTextWithHint("##dirname", "New directory name", tmp, sizeof(tmp));
                ImGui::SameLine();
                if (ImGui::Button("Create")) 
                {
                    //StaticString<MAX_PATH_LENGTH> fullPath(curDir, "/", tmp);
                    //if (!Platform::MakeDir(fullPath))
                    //    Logger::Error("Failed to create directory %s", fullPath.c_str());

                    // SetCurrentDir(curDir);  // Refresh
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

        void OnFileColumnGUI()
        {
            ImGui::BeginChild("file_col");
            float w = ImGui::GetContentRegionAvail().x;
            int columns = std::max(1, showThumbnails ? (int)w / int(TILE_SIZE * thumbnailSize) : 1);
            int tileCount = fileInfos.size();
            int rows = showThumbnails ? (tileCount + columns - 1) / columns : tileCount;

            auto HandleResource = [this](AssetItem& tile, int idx) {
                if (ImGui::IsItemHovered()) 
                    ImGui::SetTooltip("%s", tile.filepath.c_str());

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::Text("%s", (const char*)tile.filepath);
                    ImGui::SetDragDropPayload("ResPath", tile.filepath, StringLength(tile.filepath) + 1, ImGuiCond_Once);
                    ImGui::EndDragDropSource();
                }
                else if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseDoubleClicked(0))
                        DoubleClickResource(Path(tile.filepath));
                    else if (ImGui::IsMouseReleased(0))
                        SelectResource(Path(tile.filepath), true);
                    else if (ImGui::IsMouseReleased(1))
                    {
                        popupCtxResource = idx;
                        ImGui::OpenPopup("itemCtx");
                    }
                }
            };

            auto GetThumbnailIndex = [this](int i, int j, int columns) {
                int idx = j * columns + i;
                if (idx >= (int)fileInfos.size()) {
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

                            AssetItem& tile = fileInfos[idx];
                            if (tile.type == AssetItemType::Directory)
                            {
                                ShowThumbnail(tile, thumbnailSize * TILE_SIZE, false);
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
                                }
                            }
                            else if (tile.type == AssetItemType::Resource)
                            {
                                //bool selected = selectedResources.find([&](ResPtr<Resource>& res) {
                                //    return res->GetPath().GetHash() == tile.filePathHash;
                                //}) >= 0;
                                bool selected = false;
                                ShowThumbnail(tile, thumbnailSize * TILE_SIZE, selected);
                                HandleResource(tile, idx);
                            }
                        }
                    }
                    else
                    {
                        AssetItem& tile = fileInfos[j];
                        //bool selected = selectedResources.find([&](ResPtr<Resource>& res) {
                        //    return res->GetPath().GetHash() == tile.filePathHash;
                        //}) >= 0;
                        bool selected = false;
                        ImGui::Selectable(tile.filepath, selected);
                        HandleResource(tile, j);
                    }
                }
            }

            // Popup menu
            bool openDeletePopup = false;
            if (ImGui::BeginPopup("itemCtx"))
            {
                ImGui::Text("%s", fileInfos[popupCtxResource].filename.data);
                ImGui::Separator();
                if (ImGui::MenuItem("Open"))
                {
                    // TODO Open a resource viewer
                }

                if (ImGui::MenuItem(ICON_FA_EXTERNAL_LINK_ALT "Open externally"))
                    OpenInExternalEditor(fileInfos[popupCtxResource].filepath);

                if (ImGui::BeginMenu("Rename"))
                {
                    static char tmp[MAX_PATH_LENGTH] = "";
                    ImGui::InputTextWithHint("##New name", "New name", tmp, sizeof(tmp));
                    if (ImGui::Button("Rename", ImVec2(100, 0)))
                    {
                        PathInfo pathInfo(fileInfos[popupCtxResource].filepath);
                        StaticString<MAX_PATH_LENGTH> newPath(pathInfo.dir, tmp, ".", pathInfo.extension);
                        FileSystem::MoveFile(fileInfos[popupCtxResource].filepath.c_str(), newPath.c_str());
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
                popupCtxResource = -1;
                ShowCommonPopup();
                ImGui::EndPopup();
            }

            // Open delete popup
            if (openDeletePopup) 
                ImGui::OpenPopup("Delete_file");
            if (ImGui::BeginPopupModal("Delete_file", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Are you sure to delete %s? This can not be undone.", fileInfos[popupCtxResource].filename.c_str());
                if (ImGui::Button("Yes", ImVec2(100, 0))) 
                {           
                    DeleteResTile(popupCtxResource);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(ImGui::GetWindowWidth() - 100 - ImGui::GetStyle().WindowPadding.x);
                if (ImGui::Button("Cancel", ImVec2(100, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::EndChild();
        }




        bool CopyThumbnailTile(const char* from, const char* to)
        {
            OutputMemoryStream mem;
            if (!FileSystem::LoadContext(from, mem))
                return false;

            return ResourceImportingManager::Create([&](CreateResourceContext& ctx)->CreateResult {
                IMPORT_SETUP(Texture);

                // Texture header
                TextureHeader header = {};
                header.type = TextureResourceType::TGA;
                ctx.WriteCustomData(header);

                // Texture data
                auto data = ctx.AllocateChunk(0);
                data->mem.Write(mem.Data(), mem.Size());

                // Save
                return CreateResult::Ok;
            }, Path(from), Path(to));
        }

        void RefreshThumbnail(AssetItem& info)
        {
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
    };

    UniquePtr<AssetBrowser> AssetBrowser::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<AssetBrowserImpl>(app);
    }
}
}