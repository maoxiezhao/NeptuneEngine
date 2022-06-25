#include "assetCompiler.h"
#include "editor\editor.h"
#include "core\platform\platform.h"
#include "imgui-docking\imgui.h"

namespace VulkanTest
{

template<>
struct HashMapHashFunc<Path>
{
    static U32 Get(const Path& key)
    {
        const U64 hash = key.GetHashValue();
        return U32(hash ^ (hash >> 32));
    }
};

namespace Editor
{
    class AssetCompilerImpl;

    constexpr U32 COMPRESSION_SIZE_LIMIT = 4096;
    constexpr U32 MAX_PROCESS_COMPILED_JOB_COUNT = 16;

    #define EXPORT_RESOURCE_LIST ".export/resources/_list.list"
    #define EXPORT_RESOURCE_LIST_TEMP ".export/resources/_list.list_temp"
    #define EXPORT_RESOURCE_VERSION ".export/resources/_version.vs"

    struct CompileJob
    {
        U32 generation;
        Path path;
    };

    struct LoadHook : public ResourceManager::LoadHook
    {
        LoadHook(AssetCompilerImpl& compiler_) : compiler(compiler_) {}

        Action OnBeforeLoad(Resource& res)override;
        AssetCompilerImpl& compiler;
    };

    class CompilerTask final : public Thread
    {
    public:
        CompilerTask(AssetCompilerImpl& impl_) :
            impl(impl_)
        {}

        I32 Task() override;

        AssetCompilerImpl& impl;
        bool isFinished = false;
    };

    class AssetCompilerImpl : public AssetCompiler
    {
    public:
        EditorApp& editor;
        HashMap<U64, IPlugin*> plugins;
        HashMap<U64, U32> resGenerations;
        HashMap<U32, ResourceType> registeredExts;

        Array<CompileJob> toCompileJobs;
        Array<CompileJob> compiledJobs;
        Mutex toCompileMutex;
        Mutex compiledMutex;
        HashMap<Path, Array<Path>> dependencies;
        U32 batchCompileCount = 0;
        U32 batchRemainningCount = 0;

        Mutex mutex;
        CompilerTask compilerTask;
        Semaphore semaphore;
        LoadHook lookHook;

        UniquePtr<Platform::FileSystemWatcher> watcher;
        Mutex changedMutex;
        Array<Path> changedDirs;
        Array<Path> changedFiles;
        HashMap<Path, bool> changedMap;
        DelegateList<void(const Path& path)> onListChanged;

        Mutex resMutex;
        HashMap<U64, ResourceItem> resources;

        Array<Resource*> onInitLoad;

        Path inprogressRes;
        bool initialized = false;

    public:
        AssetCompilerImpl(EditorApp& editor_) : 
            editor(editor_),
            lookHook(*this),
            semaphore(0, 0xFFff),
            compilerTask(*this)
        {
            Engine& engine = editor.GetEngine();
            FileSystem& fs = engine.GetFileSystem();

            watcher = Platform::FileSystemWatcher::Create(fs.GetBasePath());
            watcher->GetCallback().Bind<&AssetCompilerImpl::OnFileChanged>(this);

            // Setup compiler task
            compilerTask.Create("CompilerTask");

            // Check the export director
            const char* basePath = fs.GetBasePath();
            StaticString<MAX_PATH_LENGTH> path(basePath, ".export/resources");
            if (!Platform::DirExists(path.c_str()))
            {
                if (!Platform::MakeDir(path.c_str()))
                {
                    Logger::Error("Failed to create the directory of export resource:%s", path.c_str());
                    return;
                }

                // Create version file
                auto file = fs.OpenFile(EXPORT_RESOURCE_VERSION, FileFlags::DEFAULT_WRITE);
                if (!file->IsValid())
                {
                    Logger::Error("Failed to create the version file of resources");
                    return;
                }

                file->Write(0);
                file->Close();
            }

            // Check the version file
            auto file = fs.OpenFile(EXPORT_RESOURCE_VERSION, FileFlags::DEFAULT_READ);
            if (!file->IsValid())
            {
                Logger::Error("Failed to load the version file of resources");
                return;
            }
            U32 version = 0;
            file->Read(version);
            file->Close();
            if (version != 0)
            {
                Logger::Warning("Invalid version of resources");
                return;
            }

            // Set load hook of the resource manager
            ResourceManager& resManager = engine.GetResourceManager();
            resManager.SetLoadHook(&lookHook);
        }

        ~AssetCompilerImpl()
        {
            Engine& engine = editor.GetEngine();
            FileSystem& fs = engine.GetFileSystem();
            auto file = fs.OpenFile(EXPORT_RESOURCE_LIST_TEMP, FileFlags::DEFAULT_WRITE);
            if (!file->IsValid())
            {
                Logger::Error("Failed to save the list of resources");
                return;
            }

            // Record all resources
            // resource = {
            //   ResPath1,
            //   ResPath2
            // }
            file->Write("resources = {\n");
            for (auto& item : resources)
            {
                file->Write("\"");
                file->Write(item.path.c_str());
                file->Write("\",\n");
            }
            file->Write("}\n\n");
            
            // dependencies = {
            //   ["Path1"] = { depPath1, depPath2 },
            //   ["Path2"] = { depPath3, depPath4 }
            // }
            file->Write("dependencies = {\n");
            for (auto it = dependencies.begin(); it != dependencies.end(); ++it)
            {
                file->Write("\t[\"");
                file->Write(it.key().c_str());
                file->Write("\"] = {\n");
                for (const Path& path : it.value())
                {
                    file->Write("\t\t\"");
                    file->Write(path.c_str());
                    file->Write("\",\n");
                }
                file->Write("\t},\n");
            }
            file->Write("}\n\n");
            file->Close();
            fs.DeleteFile(EXPORT_RESOURCE_LIST);
            fs.MoveFile(EXPORT_RESOURCE_LIST_TEMP, EXPORT_RESOURCE_LIST);

            compilerTask.isFinished = true;
            semaphore.Signal();
            compilerTask.Destroy();

            ResourceManager& resManager = engine.GetResourceManager();
            resManager.SetLoadHook(nullptr);
        }

        void InitFinished() override
        {
            initialized = true;
         
            for (Resource* res : onInitLoad)
            {
                PushToCompileQueue(res->GetPath());
                res->Release();
            }
            onInitLoad.clear();

            // Load EXPORT_RESOURCE_LIST
            LoadResourceList();

            // Check current base directroy
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            const U64 listLastmodified = fs.GetLastModTime(EXPORT_RESOURCE_LIST);
            ProcessDirectory("", listLastmodified);
        }

        void LoadResourceList()
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            OutputMemoryStream mem;
            if (fs.LoadContext(EXPORT_RESOURCE_LIST, mem))
            {
                lua_State* l = luaL_newstate();
                [&]() {
                    if (!LuaUtils::LoadBuffer(l, (const char*)mem.Data(), mem.Size(), "resource_list"))
                    {
                        Logger::Error("Failed to load resource list:%s", lua_tostring(l, -1));
                        return;
                    }

                    lua_getglobal(l, "resources");
                    if (lua_type(l, -1) != LUA_TTABLE)
                        return;

                    resMutex.Lock();
                    LuaUtils::ForEachArrayItem<Path>(l, -1, "resource list expected", [this, &fs](const Path& p) {
                        ResourceType resType = GetResourceType(p.c_str());
                        if (resType != ResourceType::INVALID_TYPE)
                        {
                            if (fs.FileExists(p.c_str()))
                            {
                                resources.insert(p.GetHashValue(), {
                                    p,
                                    resType,
                                    GetDirHash(p.c_str())
                                });
                            }
                            else
                            {
                                // Remove the export file if the original dose not exist
                                MaxPathString exportPath(".export/Resources/", p.GetHashValue(), ".res");
                            }
                        }
                    });
                    resMutex.Unlock();
                    lua_pop(l, 1);

                    // Parse dependencies
                    // dependencies = {
                    //   ["Path1"] = { depPath1, depPath2 },
                    //   ["Path2"] = { depPath3, depPath4 }
                    // }
                    lua_getglobal(l, "dependencies");
                    if (lua_type(l, -1) != LUA_TTABLE)
                        return;

                    lua_pushnil(l);
                    while (lua_next(l, -2) != 0)
                    {
                        if (!lua_isstring(l, -2) || !lua_istable(l, -1)) 
                        {
                            Logger::Error("Invalid dependencies in resource list");
                            lua_pop(l, 1);
                            continue;
                        }

                        const char* key = lua_tostring(l, -2);
                        const Path path(key);
                        Array<Path>& deps = dependencies.emplace(path).value();
                        LuaUtils::ForEachArrayItem<Path>(l, -1, "path list expected", [&deps](const Path& p) {
                            deps.push_back(p);
                        });
                        lua_pop(l, 1);
                    }

                    lua_pop(l, 1);
                }();
                lua_close(l);
            }
        }

        void Update(F32 dt) override
        {
            // Process compiled jobs
            for (int i = 0; i < MAX_PROCESS_COMPILED_JOB_COUNT; i++)
            {
                CompileJob compiled = PopCompiledQueue();
                if (compiled.path.IsEmpty())
                    break;

                // Check the current generation of resource
                U32 generation = 0;
                {
                    ScopedMutex lock(toCompileMutex);
                    auto it = resGenerations.find(compiled.path.GetHashValue());
                    generation = it.isValid() ? it.value() : 0;
                }
                if (generation != compiled.generation)
                    continue;

                // Continue loading resource
                ScopedMutex lock(compiledMutex);
                for (const auto& it : resources)
                {
                    if (!EndsWith(it.path.c_str(), compiled.path.c_str()))
                        continue;

                    Resource* res = GetResource(compiled.path);
                    if (res && res->IsHooked() && (!res->IsFailure() || !res->IsReady()))
                        lookHook.ContinueLoad(*res);
                }

                // Compile remaining dependents
                auto it = dependencies.find(compiled.path);
                if (it.isValid())
                {
                    for (const Path& dep : it.value())
                        PushToCompileQueue(dep);
                }
            }

            // Handle changed dirs
            HandleChangedDirs();

            // Handle changed files
            HandleChangedFiles();
        }

        void HandleChangedDirs()
        {
            for (;;)
            {
                Path targetPath;
                {
                    ScopedMutex lock(changedMutex);
                    if (changedDirs.empty())
                        break;

                    targetPath = changedDirs.back();
                    changedDirs.pop_back();
                    changedMap.erase(targetPath);
                }

                if (!targetPath.IsEmpty())
                {
                    FileSystem& fs = editor.GetEngine().GetFileSystem();
                    const U64 listLastmodified = fs.GetLastModTime(EXPORT_RESOURCE_LIST);
                    const StaticString<MAX_PATH_LENGTH> fullPath(fs.GetBasePath(), targetPath.c_str());

                    // Check directory is deleted
                    if (Platform::DirExists(fullPath.c_str()))
                    {
                        ProcessDirectory(targetPath.c_str(), listLastmodified);
                        onListChanged.Invoke(targetPath);
                    }
                    else
                    {
                        // Remove all resources under the deleted directory
                        ScopedMutex lock(resMutex);
                        resources.eraseIf([&](const ResourceItem& ri) {
                            if (!StartsWith(ri.path.c_str(), targetPath.c_str()))
                                return false;
                            return true;
                        });
                        onListChanged.Invoke(targetPath);
                    }
                }
            }
        }

        void HandleChangedFiles()
        {
            for (;;)
            {
                Path targetPath;
                {
                    ScopedMutex lock(changedMutex);
                    if (changedFiles.empty())
                        break;

                    targetPath = changedFiles.back();
                    changedFiles.pop_back();
                    changedMap.erase(targetPath);
                }

                if (EqualString(Path::GetExtension(targetPath.ToSpan()).data(), "meta"))
                {
                    char tmp[MAX_PATH_LENGTH];
                    CopyNString(Span(tmp), targetPath.c_str(), targetPath.Length() - 5);    // remove ".meta"
                    targetPath = tmp;
                }

                FileSystem& fs = editor.GetEngine().GetFileSystem();
                auto resType = GetResourceType(targetPath.c_str());
                if (resType != ResourceType::INVALID_TYPE)
                {
                    bool isExists = fs.FileExists(targetPath.c_str());
                    if (isExists)
                    {
                        RegisterResource(targetPath.c_str());
                        PushToCompileQueue(targetPath);
                    }
                    else
                    {
                        ScopedMutex lock(resMutex);
                        resources.eraseIf([&](const ResourceItem& ri) {
                            if (!EndsWith(ri.path.c_str(), targetPath.c_str()))
                                return false;
                            return true;
                        });
                        onListChanged.Invoke(targetPath);
                    }
                }
                else
                {
                    auto it = dependencies.find(targetPath);
                    if (it.isValid())
                    {
                        for (const Path& dep : it.value())
                            PushToCompileQueue(dep);
                    }
                }
            }
        }

        void AddDependency(const Path& parent, const Path& dep)override
        {
            auto it = dependencies.find(parent);
            if (!it.isValid())
            {
                dependencies.insert(parent, std::move(Array<Path>()));
                it = dependencies.find(parent);
            }

            Array<Path>& deps = it.value();
            if (deps.indexOf(dep) < 0)
                deps.push_back(dep);
            
        }

        void EndFrame() override
        {
        }

        void OnGUI() override
        {
            if (batchRemainningCount == 0)
                return;

            const F32 uiWidth = std::max(300.f, ImGui::GetIO().DisplaySize.x * 0.33f);
            const ImVec2 pos = ImGui::GetMainViewport()->Pos;
            ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - uiWidth) * 0.5f + pos.x, 30 + pos.y));
            ImGui::SetNextWindowSize(ImVec2(uiWidth, -1));
            ImGui::SetNextWindowSizeConstraints(ImVec2(-FLT_MAX, 0), ImVec2(FLT_MAX, 200));
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoFocusOnAppearing
                | ImGuiWindowFlags_NoInputs
                | ImGuiWindowFlags_NoNav
                | ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);

            if (ImGui::Begin("Resource compilation", nullptr, flags)) 
            {
                ImGui::Text("%s", "Compiling resources...");
                ImGui::ProgressBar(((float)batchCompileCount - batchRemainningCount) / batchCompileCount);

                // Show the current res in progress
                StaticString<MAX_PATH_LENGTH> path;
                {
                    ScopedMutex lock(toCompileMutex);
                    path = inprogressRes.c_str();
                }
                ImGui::TextWrapped("%s", path.data);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        const char* GetName() override
        {
            return "AssetCompiler";
        }

        void OnFileChanged(const char* path)
        {
            if (StartsWith(path, "."))
                return;
            if (EndsWith(path, ".log") || EndsWith(path, ".ini"))
                return;

            const char* basePath = editor.GetEngine().GetFileSystem().GetBasePath();
            const StaticString<MAX_PATH_LENGTH> fullPath(basePath, "/", path);

            if (Platform::DirExists(fullPath.c_str()))
            {
                ScopedMutex lock(changedMutex);
                if (!changedMap.find(Path(path)).isValid())
                {
                    changedDirs.push_back(Path(path));
                    changedMap.insert(Path(path), true);
                }
            }
            else
            {
                ScopedMutex lock(changedMutex);
                if (!changedMap.find(Path(path)).isValid())
                {
                    changedFiles.push_back(Path(path));
                    changedMap.insert(Path(path), true);
                }
            }
        }

        Resource* GetResource(const Path& path)
        {
            Resource* ret = nullptr;
            ResourceManager& resManager = editor.GetEngine().GetResourceManager();
            for (auto kvp : resManager.GetAllFactories())
            {
                ret = kvp.second->GetResource(path);
                if (ret != nullptr)
                    return ret;
            }
            return ret;
        }

        bool Compile(const Path& path)override
        {
            IPlugin* plugin = GetPlugin(path);
            if (plugin == nullptr)
            {
                Logger::Error("Unknown resouce type:%s", path.c_str());
                return false;
            }
            return plugin->Compile(path);
        }

        bool CopyCompile(const Path& path)override
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            OutputMemoryStream mem;
            if (!fs.LoadContext(path.c_str(), mem))
            {
                Logger::Error("failed to read file:%s", path.c_str());
                return false;
            }

            return WriteCompiled(path.c_str(), Span(mem.Data(), mem.Size()));
        }

        bool WriteCompiled(const char* path, Span<const U8> data)override
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            OutputMemoryStream compressedData;
            U64 compressedSize = 0;
            if (data.length() > COMPRESSION_SIZE_LIMIT)
            {
                // Compress data if data is too large
                ASSERT(0);
                // TODO
            }

            Path filePath(path);
            MaxPathString exportPath(".export/Resources/", filePath.GetHashValue(), ".res");
            auto file = fs.OpenFile(exportPath.c_str(), FileFlags::DEFAULT_WRITE);
            if (!file)
            {
                Logger::Error("Failed to create export asset %s", path);
                return false;
            }

            CompiledResourceHeader header;
            header.version = CompiledResourceHeader::VERSION;
            header.originSize = (U32)data.length();
            header.isCompressed = compressedSize > 0;
            if (header.isCompressed)
            {
                file->Write(&header, sizeof(header));
                file->Write(compressedData.Data(), compressedSize);
            }
            else
            {
                file->Write(&header, sizeof(header));
                file->Write(data.data(), data.length());
            }

            file->Close();
            return true;
        }

        ResourceType GetResourceType(const char* path) const override
        {
            Span<const char> ext = Path::GetExtension(Span(path, StringLength(path)));
            alignas(U32) char tmp[6] = {};
            CopyString(tmp, ext);
            U32 key = *(U32*)tmp;
            auto it = registeredExts.find(key);
            if (!it.isValid())
                return ResourceType::INVALID_TYPE;

            return it.value();
        }

        void RegisterExtension(const char* extension, ResourceType type)override
        {
            alignas(U32) char tmp[6] = {};
            CopyString(tmp, extension);
            U32 key = *(U32*)tmp;
            registeredExts.insert(key, type);
        }

        static RuntimeHash GetDirHash(const char* path) 
        {
            char tmp[MAX_PATH_LENGTH];
            CopyString(Span(tmp), Path::GetDir(path));
            Span<const char> dir(tmp, StringLength(tmp));
            if (dir.pEnd > dir.pBegin && (*(dir.pEnd - 1) == '\\' || *(dir.pEnd - 1) == '/')) {
                --dir.pEnd;
            }
            return RuntimeHash(dir.begin(), (U32)dir.length());
        }

        void AddResource(ResourceType type, const char* path) override
        {
            const Path pathObj(path);
            ScopedMutex lock(resMutex);
            auto it = resources.find(pathObj.GetHashValue());
            if (it.isValid())
            {
                resources[pathObj.GetHashValue()] = {
                    pathObj,
                    type,
                    GetDirHash(path)
                };
            }
            else
            {
                resources.insert(pathObj.GetHashValue(), {
                    pathObj,
                    type,
                    GetDirHash(path)
                });
                onListChanged.Invoke(pathObj);
            }
        }

        const HashMap<U64, ResourceItem>& LockResources()override
        {
            resMutex.Lock();
            return resources;
        }

        void UnlockResources()override
        {
            resMutex.Unlock();
        }

        void AddPlugin(IPlugin& plugin, const std::vector<const char*>& exts) override
        {
            for (const auto& ext : exts)
            {
                const RuntimeHash hash(ext);
                ScopedMutex lock(mutex);
                plugins.insert(hash.GetHashValue(), &plugin);
            }
        }

        void AddPlugin(IPlugin& plugin) override
        {
            const auto& exts = plugin.GetSupportExtensions();
            AddPlugin(plugin, exts);
        }

        void RemovePlugin(IPlugin& plugin) override
        {
            ScopedMutex lock(mutex);
            bool finished = true;
            do 
            {
                finished = true;
                for (auto iter = plugins.begin(); iter != plugins.end(); ++iter)
                {
                    if (iter.value() == &plugin)
                    {
                        plugins.erase(iter);
                        finished = false;
                        break;
                    }
                }
            } 
            while (!finished);
        }

        // Get a taget plugin according to the extension of path
        IPlugin* GetPlugin(const Path& path)
        {
            Span<const char> ext = Path::GetExtension(path.ToSpan());
            const RuntimeHash hash(ext.data(), (U32)ext.length());
            ScopedMutex lock(mutex);
            auto it = plugins.find(hash.GetHashValue());
            return it.isValid() ? it.value() : nullptr;
        }

        void PushToCompileQueue(const Path& path)
        {
            ScopedMutex lock(toCompileMutex);
            auto it = resGenerations.find(path.GetHashValue());
            if (it.isValid() == false)
                it = resGenerations.insert(path.GetHashValue(), 0);
            else
                it.value()++;

            CompileJob job;
            job.generation = it.value();
            job.path = path;

            toCompileJobs.push_back(job);
            batchCompileCount++;
            batchRemainningCount++;
            semaphore.Signal();
        }

        CompileJob PopCompiledQueue()
        {
            ScopedMutex lock(compiledMutex);
            if (compiledJobs.empty())
                return {};

            CompileJob compiled = compiledJobs.back();
            compiledJobs.pop_back();

            batchRemainningCount--;
            if (batchRemainningCount == 0)
                batchCompileCount = 0;

            return compiled;
        }

        void RegisterResource(const char* path)
        {
            Span<const char> ext = Path::GetExtension(Span(path, StringLength(path)));
            const RuntimeHash hash(ext.data(), (U32)ext.length());
            auto it = plugins.find(hash.GetHashValue());
            if (!it.isValid())
                return;

            it.value()->RegisterResource(*this, path);
        }

        ResourceManager::LoadHook::Action OnBeforeLoad(Resource& res)
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            const char* filePath = res.GetPath().c_str();
            if (!fs.FileExists(filePath))
                return ResourceManager::LoadHook::Action::IMMEDIATE;

            if (StartsWith(filePath, ".export/resources/"))
                return ResourceManager::LoadHook::Action::IMMEDIATE;
         
            const U64 hash = res.GetPath().GetHashValue();
            const StaticString<MAX_PATH> dstPath(".export/resources/", hash, ".res");
            const StaticString<MAX_PATH> metaPath(filePath, ".meta");

            // It is a new resource or expired
            if (!fs.FileExists(dstPath) || 
                fs.GetLastModTime(dstPath) < fs.GetLastModTime(filePath) ||
                fs.GetLastModTime(dstPath) < fs.GetLastModTime(metaPath))
            {
                if (GetPlugin(res.GetPath()) == nullptr)
                    ResourceManager::LoadHook::Action::IMMEDIATE;

                // Will process after initialized
                if (initialized == false)
                {
                    res.AddReference();
                    onInitLoad.push_back(&res);
                    return ResourceManager::LoadHook::Action::DEFERRED;
                }

                // Pending this resource and wait for compiling
                PushToCompileQueue(res.GetPath());
                return ResourceManager::LoadHook::Action::DEFERRED;
            }

            return ResourceManager::LoadHook::Action::IMMEDIATE;
        }

        void ProcessDirectory(const char* dir, U64 listLastModified)
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            auto fileList = fs.Enumerate(dir);
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.filename[0] == '.')
                    continue;

                if (fileInfo.type == PathType::Directory)
                {
                    char childPath[MAX_PATH];
                    CopyString(childPath, dir);
                    if (dir[0])
                        CatString(childPath, "/");
                    CatString(childPath, fileInfo.filename);
                    ProcessDirectory(childPath, listLastModified);
                }
                else
                {
                    char fullpath[MAX_PATH];
                    CopyString(fullpath, dir);
                    if (dir[0])
                        CatString(fullpath, "/");
                    CatString(fullpath, fileInfo.filename);

                    if (fs.GetLastModTime(fullpath) > listLastModified) {
                        RegisterResource(fullpath);
                    }
                    else
                    {
                        auto it = resources.find(Path(fullpath).GetHashValue());
                        if (!it.isValid())
                            RegisterResource(fullpath);
                    }
                }
            }
        }

        DelegateList<void(const Path& path)>& GetListChangedCallback()
        {
            return onListChanged;
        }
    };

    ResourceManager::LoadHook::Action LoadHook::OnBeforeLoad(Resource& res)
    {
        return compiler.OnBeforeLoad(res);
    }

    I32 CompilerTask::Task()
    {
        while (!isFinished)
        {
            impl.semaphore.Wait();

            CompileJob job;
            {
                // Get the next job to compile
                ScopedMutex lock(impl.toCompileMutex);
                if (!impl.toCompileJobs.empty())
                {
                    CompileJob temp = impl.toCompileJobs.back();
                    impl.toCompileJobs.pop_back();

                    if (temp.path.IsEmpty())
                        continue;

                    auto it = impl.resGenerations.find(temp.path.GetHashValue());
                    if (it.isValid() && it.value() == temp.generation) 
                    {
                        impl.inprogressRes = temp.path;
                        job = temp;
                    }   
                }
            }

            if (!job.path.IsEmpty())
            {
                PROFILE_BLOCK("Compile asset");
                if (!impl.Compile(job.path))
                    Logger::Error("Failed to compile resource:%s", job.path.c_str());

                ScopedMutex lock(impl.compiledMutex);
                impl.compiledJobs.push_back(job);
            }
        }

        return 0;
    }

    void AssetCompiler::IPlugin::RegisterResource(AssetCompiler& compiler, const char* path)
    {
        ResourceType type = compiler.GetResourceType(path);
        if (type == ResourceType::INVALID_TYPE)
            return;
        
        compiler.AddResource(type, path);
    }

    UniquePtr<AssetCompiler> AssetCompiler::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<AssetCompilerImpl>(app);
    }
}
}