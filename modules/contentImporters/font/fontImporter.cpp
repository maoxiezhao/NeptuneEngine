#include "fontImporter.h"
#include "renderer\render2D\fontResource.h"

namespace VulkanTest
{
namespace Editor
{
    namespace FontImporter
    {
        CreateResult Import(CreateResourceContext& ctx)
        {
            IMPORT_SETUP(FontResource);
            ctx.skipMetadata = true;

            OutputMemoryStream mem;
            if (!FileSystem::LoadContext(ctx.input, mem))
                return CreateResult::Error;

            // Write font header
            FontResource::FontHeader header;
            ctx.WriteCustomData(header);

            // Load source code
            auto chunk = ctx.AllocateChunk(0);
            if (chunk == nullptr)
                return CreateResult::AllocateFailed;
            chunk->mem.Write(mem.Data(), mem.Size() + 1);

            return CreateResult::Ok;
        }
    }
}
}