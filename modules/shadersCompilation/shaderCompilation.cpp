#include "shaderCompilation.h"
#include "core\engine.h"
#include "core\utils\helper.h"
#include "core\profiler\profiler.h"
#include "core\platform\platform.h"

#ifdef CJING3D_RENDERER_VULKAN
#include "shaderCompiler_Vulkan.h"
#endif

namespace VulkanTest
{
    struct ShaderParser : public IShaderParser, public ITokenReadersContainer<ShaderMetaCollector>
    {
        OutputMemoryStream& mem;
        const Array<GPU::ShaderMacro>& macros;
        TextReader textReader;

        ShaderParser(OutputMemoryStream& mem_, const Array<GPU::ShaderMacro>& macros_) :
            mem(mem_),
            macros(macros_),
            textReader((const char*)mem.Data(), (U32)mem.Size())
        {
            readers.push_back(CJING_NEW(VertexShaderFunctionReader));
            readers.push_back(CJING_NEW(PixelShaderFunctionReader));
            readers.push_back(CJING_NEW(ComputeShaderFunctionReader));
        }

        ~ShaderParser()
        {
        }

        void CollectResults(ShaderMeta& shaderMeta)
        {
            for (auto reader : readers)
                reader->CollectResults(*this, &shaderMeta);
        }

        bool Process(ShaderMeta& shaderMeta)
        {
            const TextReader::Separator singleLineCommentSeparator('/', '/');
            const TextReader::Separator multiLineCommentSeparator('/', '*');

            TextReader::Token token;
            while (textReader.CanRead())
            {
                textReader.ReadToken(token);

                // Skip comments
                // Single line comment
                if (token.separator == singleLineCommentSeparator)
                {
                    textReader.ReadLine();
                }
                // Multi line comment
                else if (token.separator == multiLineCommentSeparator)
                {
                    char prev = ' ';
                    char c;
                    while (textReader.CanRead())
                    {
                        c = textReader.ReadChar();
                        if (prev == '*' && c == '/')
                            break;

                        prev = c;
                    }

                    if (!textReader.CanRead())
                        Logger::Warning("Missing multiline comment ending");
                }
                // Skip definition
                else if (
                    token == TextReader::Token("#include") ||
                    token == TextReader::Token("#define"))
                {
                    textReader.ReadLine();
                }
                else
                {
                    if (!ProcessChildren(token, *this))
                        textReader.ReadLine();
                }   
            }

            CollectResults(shaderMeta);
            return true;
        }

        TextReader& GetTextReader() override
        {
            return textReader;
        }
    };

    bool ParseShader(OutputMemoryStream& mem, const Array<GPU::ShaderMacro>& macros, ShaderMeta& shaderMeta)
    {
        PROFILE_FUNCTION();
        ShaderParser parser(mem, macros);
        return parser.Process(shaderMeta);
    }

    bool CompileShaders(ShaderCompilationContext& context)
    {
        ASSERT(context.options->outMem);
        auto& output = *context.options->outMem;

        // Shader count
        U32 shaderCount = context.shaderMeta->GetShadersCount();
        output.Write(shaderCount);

        // Compile shaders
        ShaderCompilerVulkan shaderCompiler(context);
        return shaderCompiler.CompileShaders();
    }

    bool ShaderCompilation::Compile(CompilationOptions& options)
    {
        PROFILE_FUNCTION();

        if (options.targetName.empty() || !options.targetGuid.IsValid())
            return false;

        if (options.outMem == nullptr)
            return false;

        if (options.source == nullptr || options.sourceLength <= 0)
            return false;

        // Remove null in the end of source
        while (options.sourceLength > 2 && options.source[options.sourceLength - 1] == 0)
            options.sourceLength--;

        const auto startTime = Timer::GetTimeSeconds();
        OutputMemoryStream mem;
        mem.Link((const U8*)options.source, options.sourceLength);

        // Parse shader to collect shader metadata
        ShaderMeta shaderMeta;
        if (!ParseShader(mem, options.Macros, shaderMeta))
        {
            Logger::Warning("Failed to parse shader file.");
            return false;
        }

        if (shaderMeta.GetShadersCount() == 0)
        {
            Logger::Warning("No invalid entry functions.");
            return false;
        }

        // Perform compilation
        ShaderCompilationContext context;
        context.source = &mem;
        context.options = &options;
        context.shaderMeta = &shaderMeta;
        if (!CompileShaders(context))
        {
            Logger::Warning("Failed to compile shader.");
            return false;
        }

        const auto endTime = Timer::GetTimeSeconds();
        Logger::Info("Shader compilation succeed %s in %.3f s", options.targetName.c_str(), endTime - startTime);
        return true;
    }

    bool ShaderCompilation::Write(Guid guid, const Path& inputPath, const Path& outputPath, OutputMemoryStream& mem)
    {
#if 0
        return ResourceImportingManager::Create([&](CreateResourceContext& ctx)->CreateResult {
            IMPORT_SETUP(Shader);
            
            // Write header
            Shader::FileHeader header;
            header.magic = Shader::FILE_MAGIC;
            header.version = Shader::FILE_VERSION;
            ctx.WriteCustomData(header);

            // Write source code
            auto dataChunk = ctx.AllocateChunk(0);
            if (dataChunk == nullptr)
                return CreateResult::AllocateFailed;
            dataChunk->mem.Link(mem.Data(), mem.Size());

            return CreateResult::Ok;
        }, guid, inputPath, outputPath);
#else
        return false;
#endif
    }
}