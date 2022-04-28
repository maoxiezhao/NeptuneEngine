#include "resourceCompiler.h"

namespace VulkanTest
{
    constexpr U32 COMPRESSION_SIZE_LIMIT = 4096;

    bool ResourceCompiler::CompiledResource(const char* path, Span<const U8>data, FileSystem& fs)
    {
        OutputMemoryStream compressedData;
        U64 compressedSize = 0;
        if (data.length() > COMPRESSION_SIZE_LIMIT)
        {
            // Compress data if data is too large
            ASSERT(0);
            // TODO
        }

        Path filePath(path);
        MaxPathString exportPath(".export/Resources/", filePath.GetHash(), ".res");
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
    
} // namespace VulkanTest