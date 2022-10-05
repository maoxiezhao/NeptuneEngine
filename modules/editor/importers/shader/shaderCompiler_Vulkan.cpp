#ifdef CJING3D_RENDERER_VULKAN

#include "shaderCompiler_Vulkan.h"
#include "shaderCompilation.h"
#include "gpu\vulkan\shaderCompiler.h"
#include "gpu\vulkan\shaderManager.h"
#include "core\utils\helper.h"

namespace VulkanTest
{
namespace Editor
{
	ShaderCompilerVulkan::ShaderCompilerVulkan(ShaderCompilationContext& context_) :
		context(context_),
        outMem(*context.options->outMem)
	{
	}

    ShaderCompilerVulkan::~ShaderCompilerVulkan()
    {
    }

	bool ShaderCompilerVulkan::CompileShaders()
	{
        std::string sourcedir = GPU::ShaderManager::SOURCE_SHADER_PATH;
        Helper::MakePathAbsolute(sourcedir);

        auto shaderMeta = context.shaderMeta;
        for (auto& meta : shaderMeta->vs)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::VS);
            if (!CompileShader(meta, sourcedir.c_str()))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        for (auto& meta : shaderMeta->ps)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::PS);
            if (!CompileShader(meta, sourcedir.c_str()))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        for (auto& meta : shaderMeta->cs)
        {
            ASSERT(meta.GetStage() == GPU::ShaderStage::CS);
            if (!CompileShader(meta, sourcedir.c_str()))
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                return false;
            }
        }

        return true;
	}

    bool ShaderCompilerVulkan::CompileShader(ShaderFunctionMeta& meta, const char* includeDir)
    {
        ScopedMutex lock(mutex);

        // Write shader infos
        WriteShaderInfo(meta);

        // Compile shader and write it
        GPU::ShaderCompiler::CompilerInput input;   
        input.includeDirectories.push_back(includeDir);
        input.shadersourceData = (char*)context.source->Data();
        input.shadersourceSize = (U32)context.source->Size();
        input.stage = meta.GetStage();
        input.entrypoint = meta.name;
        auto& macros = input.defines;

        for (U32 permutationIndex = 0; permutationIndex < (U32)meta.permutations.size(); permutationIndex++)
        {
            macros.clear();

            // Get macros from permutation
            meta.GetDefinitionsForPermutation(permutationIndex, macros);
            // Get global macros
            for (const auto& macro : context.options->Macros)
                macros.push_back(macro);

            // Compile shader
            GPU::ShaderCompiler::CompilerOutput output;
            if (GPU::ShaderCompiler::Compile(input, output))
            {
                Logger::Info("Compile shader successfully:%s", meta.name.c_str());
                if (!output.errorMessage.empty())
                    Logger::Error(output.errorMessage.c_str());
            }
            else
            {
                Logger::Error("Failed to compile shader %s", meta.name.c_str());
                if (!output.errorMessage.empty())
                    Logger::Error(output.errorMessage.c_str());

                return false;
            }

            // Add dependencies
            for (auto& dependency : output.dependencies)
            {
                if (dependency != "")
                    context.options->dependencies.push_back(Path(dependency.c_str()));
            }

            // Reflect shader to get resource layout
            GPU::ShaderResourceLayout resLayout;
            if (!GPU::Shader::ReflectShader(resLayout, (const U32*)output.shaderdata, output.shadersize))
            {
                Logger::Error("Failed to reflect shader %s", meta.name);
                return false;
            }

            // Write shader
            WriteShaderFunctionPermutation(meta, permutationIndex, resLayout, output.shaderdata, output.shadersize);
        }

        return true;
    }

    void ShaderCompilerVulkan::WriteShaderInfo(ShaderFunctionMeta& meta)
    {
        // Stage
        Write((U8)meta.GetStage());
        // Permutations count
        Write((U32)meta.permutations.size());
        // Function name
        Write((U32)meta.name.size());
        Write(meta.name.c_str(), meta.name.size());
    }

    void ShaderCompilerVulkan::WriteShaderFunctionPermutation(ShaderFunctionMeta& meta, I32 permutationIndex, const GPU::ShaderResourceLayout& resLayout, const void* cache, I32 cacheSize)
    {
        // CachesSize
        Write(cacheSize);
        // Cache
        Write(cache, cacheSize);
        // ResLayout
        Write(resLayout);
    }
}
}


#endif