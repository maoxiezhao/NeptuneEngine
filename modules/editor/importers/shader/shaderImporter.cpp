#include "shaderImporter.h"
#include "editor\editor.h"
#include "editor\importers\resourceImportingManager.h"

namespace VulkanTest
{
namespace Editor
{
    namespace ShaderImporter
    {
        Array<UniquePtr<Platform::FileSystemWatcher>> shaderSourceWatchers;

        Guid GetShaderGUID(const String& name)
        {
            Guid result;
            auto hash = StringID(name.c_str()).GetHashValue();
            result.A = name.length() * 100;
            result.B = (U32)(hash & 0xffFFffFF);
            result.C = (U32)((hash >> 32) & 0xffFFffFF);
            result.D = name.empty() ? 0 : name[0];
            return result;
        }

        void OnShaderWatcherEvent(const Path& path, Platform::FileWatcherAction action)
        {
            if (action == Platform::FileWatcherAction::Delete)
                return;
            if (!EndsWith(path, ".shd"))
                return;

            Logger::Info("shader %s has been modified", path);

            // Wait a little so app that was editing the file (e.g. Visual Studio, Notepad++) has enough time to flush whole file change
            Platform::Sleep(0.1f);

            auto pos = ReverseFindSubstring(path, "/shaders");
            if (pos < 0)
                return;

            Path projectPath = Path(String(path).substr(0, pos));
            const Path shaderResourcePath = projectPath / "content/shaders";
            const Path shaderSourcePath = projectPath / "shaders";
            const Path localPath = Path::ConvertAbsolutePathToRelative(shaderSourcePath, path);
            String filename = Path::GetPathWithoutExtension(localPath);
            const Path outputPath = shaderResourcePath / filename + RESOURCE_FILES_EXTENSION_WITH_DOT;
            Guid id = GetShaderGUID(filename);
            ResourceImportingManager::ImportIfEdited(Path(path), outputPath, id, nullptr);
        }

        void ProcessShaderDirectory(const char* dir, const Path& base, const Path& targetPath)
        {
            auto fileList = FileSystem::Enumerate(dir);
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
                    ProcessShaderDirectory(childPath, base, targetPath);
                }
                else
                {
                    if (!EndsWith(fileInfo.filename, ".shd"))
                        continue;

                    char inputPath[MAX_PATH];
                    CopyString(inputPath, dir);
                    if (dir[0])
                        CatString(inputPath, "/");
                    CatString(inputPath, fileInfo.filename);

                    const Path localPath = Path::ConvertAbsolutePathToRelative(base, Path(inputPath));
                    const String filename = Path::GetPathWithoutExtension(localPath);
                    const Path outputPath = targetPath / filename + RESOURCE_FILES_EXTENSION_WITH_DOT;
                    Guid id = GetShaderGUID(fileInfo.filename);
                    ResourceImportingManager::ImportIfEdited(Path(inputPath), outputPath, id, nullptr);
                }
            }
        }

        void RegisterShaderWatchers(const ProjectInfo* project, Array<const ProjectInfo*>& projects)
        {
            if (projects.indexOf(project) >= 0)
                return;
            projects.push_back(project);

            const Path shadersSourcePath = project->ProjectFolderPath / "shaders";
            if (Platform::DirExists(shadersSourcePath))
            {
                auto watcher = Platform::FileSystemWatcher::Create(shadersSourcePath);
                watcher->GetCallback().Bind<&OnShaderWatcherEvent>();
                shaderSourceWatchers.push_back(std::move(watcher));

                const Path shadersReourcePath = project->ProjectFolderPath / "content/shaders";
                ProcessShaderDirectory(shadersSourcePath.c_str(), shadersSourcePath, shadersReourcePath);
            }

            for (const auto& ref : project->References)
            {
                if (ref.Project)
                    RegisterShaderWatchers(ref.Project, projects);
            }
        }

        class ShaderImporterServiceImpl : public EngineService
        {
        public:
            ShaderImporterServiceImpl() :
                EngineService("ShaderImporterService", -100)
            {}

            bool Init(Engine& engine) override;
            void Uninit() override;
        };
        ShaderImporterServiceImpl ShaderImporterServiceImplInstance;

        bool ShaderImporterServiceImpl::Init(Engine& engine)
        {
            Array<const ProjectInfo*> projects;
            RegisterShaderWatchers(EditorApp::GetProject(), projects);
            return true;
        }

        void ShaderImporterServiceImpl::Uninit()
        {
            shaderSourceWatchers.clear();
        }

        CreateResult Import(CreateResourceContext& ctx)
        {
            IMPORT_SETUP(Shader);
            ctx.skipMetadata = true;

            OutputMemoryStream mem;
            if (!FileSystem::LoadContext(ctx.input, mem))
                return CreateResult::Error;
            
            // Load source code
            auto chunk = ctx.AllocateChunk(SHADER_RESOURCE_CHUNK_SOURCE);
            if (chunk == nullptr)
                return CreateResult::AllocateFailed;
            chunk->mem.Write(mem.Data(), mem.Size() + 1);

            // Write shader header
            ShaderStorage::Header header;
            header.magic = ShaderStorage::Header::FILE_MAGIC;
            header.version = ShaderStorage::Header::FILE_VERSION;
            ctx.WriteCustomData(header);

            return CreateResult::Ok;
        }
    }
}
}