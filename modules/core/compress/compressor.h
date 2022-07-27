#pragma once

#include "core\common.h"
#include "core\utils\stream.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Compressor
	{
	public:
		static bool Compress(OutputMemoryStream& compressedData, U64& compressedSize, Span<const U8> data);
		static I32  Decompress(const char* source, char* dest, int compressedSize, int maxDecompressedSize);
	};
}