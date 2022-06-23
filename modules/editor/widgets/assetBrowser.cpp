#include "assetBrowser.h"
#include "assetCompiler.h"
#include "editor\editor.h"
#include "core\platform\platform.h"
#include "imgui-docking\imgui.h"
#include "imguiUtils.h"

namespace VulkanTest
{
namespace Editor
{
    static constexpr int TILE_SIZE = 96;

    class AssetBrowserImpl : public AssetBrowser
    {
    public:
        EditorApp& editor;
        bool initialized = false;
        float leftColumnWidth = 120;
        bool showThumbnails = true;
        float thumbnailSize = 1.f;

        Array<Resource*> selectedResources;
        I32 contextResource = -1;

        MaxPathString curDir;
        Array<MaxPathString> subdirs;

        struct FileInfo 
        {
            MaxPathString filepath;
            MaxPathString filename;
            StringID filePathHash;
            GPU::ImagePtr tex;
        };
        Array<FileInfo> fileInfos;  // All file infos in the current directory

        Array<IPlugin*> plugins;

    public:
        AssetBrowserImpl(EditorApp& editor_) :
            editor(editor_)
        {
        }

        ~AssetBrowserImpl()
        {
            UnloadSelectedResources();

            editor.GetAssetCompiler().GetListChangedCallback().Unbind<&AssetBrowserImpl::OnResourceListChanged>(this);
        }

        void InitFinished() override
        {
            editor.GetAssetCompiler().GetListChangedCallback().Bind<&AssetBrowserImpl::OnResourceListChanged>(this);
            initialized = true;
        }

        void Update(F32 dt) override
        { 
        }

        void OnGUI() override
        {
            // Set default directory
            if (curDir.empty())
                SetCurrentDir(".");

            if (!isOpen)
            {
                OnDetailsGUI();
                return;
            }
            
            if (!ImGui::Begin("Assets", &isOpen)) {
                ImGui::End();
                OnDetailsGUI();
                return;
            }

            BreadCrumbs();
            ImGui::Separator();

            float contentW = ImGui::GetContentRegionAvail().x;
            ImVec2 leftSize(leftColumnWidth, 0);
            if (leftSize.x < 10) leftSize.x = 10;
            if (leftSize.x > contentW - 10) leftSize.x = contentW - 10;

            // Show left dir column
            OnDirColumnGUI();

            ImGui::SameLine();
            ImGuiUtils::VSplitter("vsplit1", &leftSize);
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

        void AddPlugin(IPlugin& plugin) override
        {
        }

        void RemovePlugin(IPlugin& plugin) override
        {
        }

        void OnResourceListChanged(const Path& path)
        {
            Span<const char> dir = Path::GetDir(path.c_str());
            if (dir.length() > 0 && (*(dir.pEnd - 1) == '/' || *(dir.pEnd - 1) == '\\'))
                --dir.pEnd; // == dir[len - 1] = '\0'

            if (!EqualString(dir, curDir.toSpan()))
                return;

            FileSystem& fs = editor.GetEngine().GetFileSystem();
            const StaticString<MAX_PATH_LENGTH> fullPath(fs.GetBasePath(), path.c_str());
            if (Platform::DirExists(fullPath.c_str()))
            {
                SetCurrentDir(curDir);
                return;
            }

            fileInfos.clear();
            AddResTile(path);
        }

        void SetCurrentDir(const char* path)
        {
            // Clear file infos
            for (auto& info : fileInfos)
                info.tex.reset();
            fileInfos.clear();

            // Remove the '/' or '\\' in last pos when called Path::GetDir
            Path::Normalize(path, curDir.toSpan());
            size_t len = StringLength(curDir);
            if (len > 0 && (curDir[len - 1] == '/' || curDir[len - 1] == '\\'))
                curDir[len - 1] = '\0';

            subdirs.clear();
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            auto fileList = fs.Enumerate(curDir.c_str());
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.filename[0] == '.' || fileInfo.type != PathType::Directory)
                    continue;
                
                subdirs.push_back(fileInfo.filename);
            }

            // Add the resource with the same directory
            RuntimeHash hash(curDir.c_str());

            auto& compiler = editor.GetAssetCompiler();
            auto& resources = compiler.LockResources();
            for (auto& res : resources)
            {
                if (res.dirHash == hash)
                    AddResTile(res.path);
            }

            compiler.UnlockResources();
        }

        void AddResTile(const Path& path)
        {
            FileInfo info = {};
            info.filePathHash = path.GetHash();
            info.filepath = path.c_str();
            info.filename = Path::GetBaseName(path.c_str()).data();

            fileInfos.push_back(info);
        }

        void OnDetailsGUI()
        {
        }

        void OnDirColumnGUI()
        {
            ImVec2 size(std::max(120.f, leftColumnWidth), 0);
            ImGui::BeginChild("left_col", size);
            ImGui::PushItemWidth(120);

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

            ImGui::PopItemWidth();
            ImGui::EndChild();
        }

        void BreadCrumbs()
        {
            // Show buttons for current directory
            if (curDir[0] != '.' || curDir[1] != 0) 
            {
                if (ImGui::Button("."))
                    SetCurrentDir(".");

                ImGui::SameLine();
                ImGui::TextUnformatted("/");
                ImGui::SameLine();
            }

            char tmp[MAX_PATH_LENGTH];
            const char* c = curDir.c_str();
            while (*c)
            {
                char* ptr = tmp;
                while (*c && *c != '/')
                {
                    *ptr = *c;
                    ++ptr;
                    ++c;
                }
                *ptr = '\0';
                if (*c == '/') ++c;

                if (ImGui::Button(tmp))
                {
                    char newDir[MAX_PATH_LENGTH];
                    CopyNString(Span(newDir), curDir, int(c - curDir.c_str()));
                    SetCurrentDir(newDir);
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("/");
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }

        void OnFileColumnGUI()
        {
            ImGui::BeginChild("file_col");
            float w = ImGui::GetContentRegionAvail().x;
            int columns = std::max(1, showThumbnails ? (int)w / int(TILE_SIZE * thumbnailSize) : 1);
            int tileCount = fileInfos.size();
            int rows = showThumbnails ? (tileCount + columns - 1) / columns : tileCount;

            auto HandleResource = [this](FileInfo& tile, int idx) {
                if (ImGui::IsItemHovered()) 
                    ImGui::SetTooltip("%s", tile.filepath.data);

                if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseReleased(0))
                        SelectResource(Path(tile.filepath));
                    else if (ImGui::IsMouseReleased(1))
                        contextResource = idx;
                }
            };

            auto GetThumbnailIndex = [this](int i, int j, int columns) {
                int idx = j * columns + i;
                if (idx >= fileInfos.size()) {
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

                            FileInfo& tile = fileInfos[idx];
                            bool selected = selectedResources.find([&](Resource* res) {
                                return res->GetPath().GetHash() == tile.filePathHash;
                            }) >= 0;
                            ShowThumbnail(tile, thumbnailSize * TILE_SIZE, selected);
                            HandleResource(tile, idx);
                        }
                    }
                    else
                    {
                        FileInfo& tile = fileInfos[j];
                        bool selected = selectedResources.find([&](Resource* res) {
                            return res->GetPath().GetHash() == tile.filePathHash;
                        }) >= 0;
                        ImGui::Selectable(tile.filepath, selected);
                        HandleResource(tile, j);
                    }
                }
            }

            ImGui::EndChild();
        }
        void ShowThumbnail(FileInfo& info, F32 size, bool selected)
        {
            ImGui::BeginGroup();
            ImVec2 imgSize(size, size);

            // Show thumbnail image
            if (info.tex)
            {
                ImGui::Image(info.tex->GetImage(), imgSize);
            }
            else
            {
                ImGuiUtils::Rect(imgSize.x, imgSize.y, 0xffffFFFF);

                // TODO:
                // Create custom thumbnail image
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

        void SelectResource(const Path& path)
        {
            auto& resManager = editor.GetEngine().GetResourceManager();
            const ResourceType resType = editor.GetAssetCompiler().GetResourceType(path.c_str());
            Resource* res = resManager.LoadResource(resType, path);
            if (res != nullptr)
                SelectResource(res);
        }

        void UnloadSelectedResources()
        {
            if (selectedResources.empty())
                return;

            for (auto res : selectedResources)
            {
                for (auto plugin : plugins)
                    plugin->OnResourceUnloaded(res);
                res->DecRefCount();
            }

            selectedResources.clear();
        }

        void SelectResource(Resource* res)
        {
            UnloadSelectedResources();
            selectedResources.push_back(res);
        }
    };

    UniquePtr<AssetBrowser> AssetBrowser::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<AssetBrowserImpl>(app);
    }
}
}