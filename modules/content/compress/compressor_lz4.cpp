#include "compressor.h"
#include "lz4\lz4.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	bool Compressor::Compress(OutputMemoryStream& compressedData, U64& compressedSize, Span<const U8> data)
	{
        PROFILE_FUNCTION();
        const I32 cap = LZ4_compressBound((I32)data.length());
        compressedData.Resize(cap);
        compressedSize = LZ4_compress_default((const char*)data.begin(), (char*)compressedData.Data(), (I32)data.length(), cap);
        if (compressedSize == 0)
            return false;

        compressedData.Resize(compressedSize);
        return true;
	}

    I32 Compressor::Decompress(const char* source, char* dest, int compressedSize, int maxDecompressedSize)
    {
        PROFILE_FUNCTION();
        return LZ4_decompress_safe(source, dest, compressedSize, maxDecompressedSize);
    }
}