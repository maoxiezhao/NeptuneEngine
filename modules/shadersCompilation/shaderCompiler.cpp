#include "shaderCompiler.h"
#include "shaderCompilation.h"
#include "gpu\vulkan\shaderManager.h"
#include "core\utils\helper.h"
#include "core\profiler\profiler.h"

#include "shaderCompilerDX.h"

#ifdef CJING3D_EDITOR
#include "editor\projectInfo.h"
#endif

namespace VulkanTest
{
    struct IncludeFile
    {
        String path;
        U64 modTime;
        OutputMemoryStream content;
    };
    HashMap<Path, IncludeFile*> includeFiles;
    Mutex includesLocker;

	ShaderCompiler::ShaderCompiler(ShaderProfile profile_) :
        profile(profile_)
	{
	}

    ShaderCompiler::~ShaderCompiler()
    {
    }

	bool ShaderCompiler::CompileShaders(ShaderCompilationContext* context_)
	{
        context = context_;
        outMem = context_->options->outMem;

        auto shaderMeta = context->shaderMeta;
        for (auto& meta : shaderMeta->vs)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::VS);
            if (!CompileShader(meta))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        for (auto& meta : shaderMeta->ps)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::PS);
            if (!CompileShader(meta))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        for (auto& meta : shaderMeta->cs)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::CS);
            if (!CompileShader(meta))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        return true;
	}

#ifdef CJING3D_EDITOR
    bool FindProject(const ProjectInfo* project, Array<const ProjectInfo*>& projects, const String& projectName, Path& path)
    {
        if (!project || projects.indexOf(project) >= 0)
            return false;
        projects.push_back(project);

        if (project->Name == projectName)
        {
            path = project->ProjectFolderPath;
            return true;
        }

        for (const auto& ref : project->References)
        {
            if (ref.Project && FindProject(ref.Project, projects, projectName, path))
                return true;
        }
        return false;
    }
#endif

    bool ShaderCompiler::GetIncludedFileSource(ShaderCompilationContext* context, const char* sourceFile, const char* includedFile, const char*& source, I32& sourceLength)
    {
        PROFILE_FUNCTION();
        source = nullptr;
        sourceLength = 0;

        // Skip to the last root start './' but preserve the leading one
        const I32 includedFileLength = StringLength(includedFile);
        for (I32 i = includedFileLength - 2; i >= 2; i--)
        {
            if (compareString(includedFile + i, "./", 2) == 0)
            {
                includedFile = includedFile + i;
                break;
            }
        }

        ScopedMutex lock(includesLocker);

        // Get target file full path
        Path path;
#ifdef CJING3D_EDITOR
        if (compareString(includedFile, "./", 2) == 0)
        {
            I32 endPos = -1;
            for (I32 i = 2; i < includedFileLength; i++)
            {
                if (includedFile[i] == '/')
                {
                    endPos = i;
                    break;
                }
            }
            if (endPos != -1)
            {
                String name(includedFile, 2, endPos - 2);
                Array<const ProjectInfo*> projcets;
                if (FindProject(ProjectInfo::EditorProject, projcets, name, path))
                {
                    // Append local path
                    const String localPath(includedFile, endPos + 1, includedFileLength - endPos - 1);
                    path = path / "shaders" / localPath;
                }
            }
        }

        if (path.IsEmpty())
            path = includedFile;
#else
        path = includedFile;
#endif
        if (!FileSystem::FileExists(path))
        {
            path = Globals::StartupFolder / "shaders" / path;
            if (!FileSystem::FileExists(path))
            {
                Logger::Error("Unknown shader source file '{0}' included in '{1}'.", includedFile, sourceFile);
                return true;
            }
        }

        IncludeFile* file = nullptr;
        if (!includeFiles.tryGet(path, file) || FileSystem::GetLastModTime(path) > file->modTime)
        {
            if (file)
            {
                CJING_SAFE_DELETE(file);
                includeFiles.erase(path);
            }

            file = CJING_NEW(IncludeFile);
            file->path = path;
            file->modTime = FileSystem::GetLastModTime(path);
            if (!FileSystem::LoadContext(path, file->content))
            {
                Logger::Error("Failed to load shader source file %s", path.c_str());
                CJING_DELETE(file);
                return false;
            }
            includeFiles.insert(path, file);
        }

        // Add includes
        if (context->includes.indexOf(path) < 0)
            context->includes.push_back(path);

        // Copy to output
        source = (const char*)file->content.Data();
        sourceLength = file->content.Size();
        return true;
    }

    void ShaderCompiler::FreeIncludeFileCache()
    {
        ScopedMutex lock(includesLocker);
        for (auto file : includeFiles)
            CJING_SAFE_DELETE(file);
        includeFiles.clear();
    }

    bool ShaderCompiler::WriteShaderInfo(ShaderFunctionMeta& meta)
    {
        // Stage
        Write((U8)meta.GetStage());
        // Permutations count
        Write((U32)meta.permutations.size());
        // Function name
        Write((U32)meta.name.size());
        Write(meta.name.c_str(), meta.name.size());

        return true;
    }

    bool ShaderCompiler::WriteShaderFunctionPermutation(ShaderFunctionMeta& meta, I32 permutationIndex, const GPU::ShaderResourceLayout& resLayout, const void* cache, I32 cacheSize)
    {
        // CachesSize
        Write(cacheSize);
        // Cache
        Write(cache, cacheSize);
        // ResLayout
        Write(resLayout);
    
        return true;
    }
}