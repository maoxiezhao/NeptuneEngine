#include "assetCompiler.h"
#include "editor\editor.h"
#include "core\platform\platform.h"
#include "core\collections\hashMap.h"

namespace VulkanTest
{
namespace Editor
{
    constexpr U32 COMPRESSION_SIZE_LIMIT = 4096;

    class AssetCompilerImpl : public AssetCompiler
    {
    private:
        EditorApp& editor;
        HashMap<U64, ResourceItem> resources;
        HashMap<U64, IPlugin*> plugins;
        Mutex mutex;
        bool initialized = false;

        struct LoadHook : ResourceManager::LoadHook
        {
            LoadHook(AssetCompilerImpl& compiler_) : compiler(compiler_) {}

            Action OnBeforeLoad(Resource& res) override
            {
                return compiler.OnBeforeLoad(res);
            }

            AssetCompilerImpl& compiler;
        };
        LoadHook lookHook;

    public:
        AssetCompilerImpl(EditorApp& editor_) : 
            editor(editor_),
            lookHook(*this)
        {
            Engine& engine = editor.GetEngine();
            FileSystem& fs = engine.GetFileSystem();
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
                path += "/_version.vs";
                auto file = fs.OpenFile(".export/resources/_version.vs", FileFlags::DEFAULT_WRITE);
                if (!file->IsValid())
                {
                    Logger::Error("Failed to create the version file of resources");
                    return;
                }

                file->Write(0);
                file->Close();
            }

            // Check the version file
            auto file = fs.OpenFile(".export/resources/_version.vs", FileFlags::DEFAULT_READ);
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
            auto file = fs.OpenFile(".export/resources/_list.list_temp", FileFlags::DEFAULT_WRITE);
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
            file->Write("resource = {\n");
            for (auto& item : resources)
            {
                file->Write("\"");
                file->Write(item.path.c_str());
                file->Write("\",\n");
            }
            file->Write("}\n\n");
            file->Close();
            fs.DeleteFile(".export/resources/_list.list");
            fs.MoveFile(".export/resources/_list.list_temp", ".export/resources/_list.list");

            ResourceManager& resManager = engine.GetResourceManager();
            resManager.SetLoadHook(nullptr);
        }

        void InitFinished() override
        {
            initialized = true;
        }

        void Update(F32 dt) override
        {
        }

        void EndFrame() override
        {
        }

        void OnGUI() override
        {
        }

        const char* GetName() override
        {
            return "AssetCompiler";
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
            header.originSize = data.length();
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

        void AddPlugin(IPlugin& plugin, const char* ext) override
        {
        }

        void RemovePlugin(IPlugin& plugin) override
        {
        }

        // Get a taget plugin according to the extension of path
        IPlugin* GetPlugin(const Path& path)
        {
            Span<const char> ext = Path::GetExtension(path.ToSpan());
            const RuntimeHash hash(ext.data(), ext.length());
            ScopedMutex lock(mutex);
            auto it = plugins.find(hash.GetHashValue());
            return it.isValid() ? it.value() : nullptr;
        }

        void PushCompileQueue(const Path& path)
        {
        }

        ResourceManager::LoadHook::Action OnBeforeLoad(Resource& res)
        {
            FileSystem& fs = editor.GetEngine().GetFileSystem();
            const char* filePath = res.GetPath().c_str();
            if (fs.FileExists(filePath))
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
            }

            return ResourceManager::LoadHook::Action::IMMEDIATE;
        }
    };

    UniquePtr<AssetCompiler> AssetCompiler::Create(EditorApp& app)
    {
        return CJING_MAKE_UNIQUE<AssetCompilerImpl>(app);
    }
}
}