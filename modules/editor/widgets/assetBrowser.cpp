#include "assetBrowser.h"
#include "assetCompiler.h"
#include "editor\editor.h"
#include "editor\plugins\createResource.h"
#include "editor\widgets\codeEditor.h"
#include "core\platform\platform.h"
#include "renderer\texture.h"
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
        I32 contextResource = -1;
        char filter[128];
        MaxPathString curDir;
        Array<MaxPathString> subdirs;

        struct FileInfo 
        {
            MaxPathString filepath;
            MaxPathString filename;
            StringID filePathHash;
            GPU::Image* tex = nullptr;
            bool isLoading = false;
        };
        Array<FileInfo> fileInfos;  // All file infos in the current directory

        CodeEditor codeEditor;
        HashMap<U64, IPlugin*> plugins;

    public:
        AssetBrowserImpl(EditorApp& editor_) :
            editor(editor_),
            codeEditor(editor_)
        {
            filter[0] = '\0';

            // Check directory of resource tiles
            const char* path = editor.GetEngine().GetFileSystem().GetBasePath();
            StaticString<MAX_PATH_LENGTH> tilePath(path, ".export/resources_tiles");
            if (!Platform::DirExists(tilePath.c_str()))
                Platform::MakeDir(tilePath.c_str());
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
            PROFILE_FUNCTION();
            for (auto kvp : plugins) 
                kvp->Update();
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
            
            if (!ImGui::Begin(ICON_FA_IMAGES "Assets##assets", &isOpen)) {
                ImGui::End();
                OnDetailsGUI();
                return;
            }

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
            ImGui::SameLine();

            // Show bread crumbs
            BreadCrumbs();
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

            char filename[MAX_PATH_LENGTH];
            CopyString(filename, Path::GetBaseName(path.c_str()));
            ClampText(filename, I32(thumbnailSize * TILE_SIZE));
            info.filename = filename;

            fileInfos.push_back(info);
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
                    ResourceType resType = editor.GetAssetCompiler().GetResourceType(path);
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

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    ImGui::Text("%s", (const char*)tile.filepath);
                    ImGui::SetDragDropPayload("ResPath", tile.filepath, StringLength(tile.filepath) + 1, ImGuiCond_Once);
                    ImGui::EndDragDropSource();
                }
                else if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseReleased(0))
                        SelectResource(Path(tile.filepath));
                    else if (ImGui::IsMouseReleased(1))
                        contextResource = idx;
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

                            FileInfo& tile = fileInfos[idx];
                            bool selected = selectedResources.find([&](ResPtr<Resource>& res) {
                                return res->GetPath().GetHash() == tile.filePathHash;
                            }) >= 0;
                            ShowThumbnail(tile, thumbnailSize * TILE_SIZE, selected);
                            HandleResource(tile, idx);
                        }
                    }
                    else
                    {
                        FileInfo& tile = fileInfos[j];
                        bool selected = selectedResources.find([&](ResPtr<Resource>& res) {
                            return res->GetPath().GetHash() == tile.filePathHash;
                        }) >= 0;
                        ImGui::Selectable(tile.filepath, selected);
                        HandleResource(tile, j);
                    }
                }
            }

            ImGui::EndChild();
        }

        // State of Thumbnail tile
        enum class TileState 
        {
            OK,
            OUTDATED,
            DELETED,
            NOT_CREATED
        };
        static TileState GetTileState(const FileInfo& info, FileSystem& fs) 
        {
            if (!fs.FileExists(info.filepath)) 
                return TileState::DELETED;
        
            StaticString<MAX_PATH_LENGTH> path(".export/resources_tiles/", info.filePathHash.GetHashValue(), ".tile");
            if (!fs.FileExists(path)) 
                return TileState::NOT_CREATED;

            MaxPathString compiledPath(".export/Resources/", info.filePathHash.GetHashValue(), ".res");
            const U64 lastModified = fs.GetLastModTime(path);
            if (lastModified < fs.GetLastModTime(info.filepath) || lastModified < fs.GetLastModTime(compiledPath))
                return TileState::OUTDATED;

            return TileState::OK;
        }

        bool CopyThumbnailTile(const char* from, const char* to)
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            OutputMemoryStream mem;
            if (!fs.LoadContext(from, mem))
                return false;

            ResourceDataWriter resWriter(Texture::ResType);

            // Texture header
            TextureHeader header = {};
            header.type = TextureResourceType::TGA;
            resWriter.WriteCustomData(header);

            // Texture data
            auto data = resWriter.GetChunk(0);
            data->mem.Write(mem.Data(), mem.Size());

            OutputMemoryStream output;
            if (!ResourceStorage::Save(output, resWriter.data))
            {
                Logger::Error("Failed to save resource storage.");
                return false;
            }

            auto file = fs.OpenFile(to, FileFlags::DEFAULT_WRITE);
            if (!file)
            {
                Logger::Error("Failed to create tile file %s", to);
                return false;
            }

            file->Write(output.Data(), output.Size());
            file->Close();
            return true;
        }

        bool CreateThumbnailTile(FileInfo& info)
        {
            if (info.isLoading)
                return false;

            info.isLoading = true;

            auto& compiler = editor.GetAssetCompiler();
            auto resType = compiler.GetResourceType(info.filepath);

            StaticString<MAX_PATH_LENGTH> tilePath(".export/resources_tiles/", info.filePathHash.GetHashValue(), ".tile");
            // Check is builtin thumnailtile
            if (resType == Shader::ResType)
                return CopyThumbnailTile("editor/textures/tile_shader.tga", tilePath.c_str());
            else if (resType == Texture::ResType)
                return CopyThumbnailTile("editor/textures/tile_texture.tga", tilePath.c_str());
            else if (resType == Material::ResType)
                return CopyThumbnailTile("editor/textures/tile_material.tga", tilePath.c_str());
            else if (resType == Model::ResType)
                return CopyThumbnailTile("editor/textures/tile_model.tga", tilePath.c_str());
            else if (resType == FontResource::ResType)
                return CopyThumbnailTile("editor/textures/tile_font.tga", tilePath.c_str());       

            return false;
        }

        void ShowThumbnail(FileInfo& info, F32 size, bool selected)
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

                // Create custom thumbnail image
                StaticString<MAX_PATH_LENGTH> path(".export/resources_tiles/", info.filePathHash.GetHashValue(), ".tile");
                auto renderInterface = editor.GetRenderInterface();
                if (renderInterface != nullptr)
                {
                    auto& fs = editor.GetEngine().GetFileSystem();
                    TileState state = GetTileState(info, fs);
                    switch (state)
                    {
                    case TileState::OK:
                        info.tex = (GPU::Image*)renderInterface->LoadTexture(Path(path));
                        break;
                    case TileState::OUTDATED:
                    case TileState::NOT_CREATED:
                        CreateThumbnailTile(info);
                        break;
                    case TileState::DELETED:
                        break;
                    default:
                        break;
                    }
                }
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
            ResPtr<Resource> res = resManager.LoadResourcePtr(resType, path);
            if (res)
            {
                UnloadSelectedResources();
                selectedResources.push_back(std::move(res));
            }
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