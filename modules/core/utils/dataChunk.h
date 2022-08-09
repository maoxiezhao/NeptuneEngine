#pragma once

#include "stream.h"

namespace VulkanTest
{
	struct VULKAN_TEST_API DataChunk
	{
	public:
		struct Location
		{
			U32 Address;
			U32 Size;
		};
		Location location;
		U64 LastAccessTime = 0;
		OutputMemoryStream mem;
		bool compressed = false;

	public:
		U8* Data() {
			return mem.Data();
		}

		const U8* Data()const {
			return mem.Data();
		}

		U64 Size()const {
			return mem.Size();
		}

		bool IsLoaded()const {
			return mem.Size() > 0;
		}

		void Unload() {
			mem.Free();
		}

		bool IsMissing() const {
			return mem.Size() == 0;
		}

		bool ExistsInFile() const {
			return location.Size > 0;
		}

		void RegisterUsage();
	};
} 
