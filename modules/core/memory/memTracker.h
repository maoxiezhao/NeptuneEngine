#pragma once

#include "allocator.h"

#ifdef VULKAN_MEMORY_TRACKER
#include <unordered_map>

namespace VulkanTest
{
	class MemoryTracker
	{
	public:
		~MemoryTracker() {
			ReportMemoryLeak();
		}

		void RecordAlloc(void* ptr, size_t size, const char* filename, int line);
		void RecordRealloc(void* ptr, void* old, size_t size, const char* filename, int line);
		void RecordFree(void* ptr);
		void ReportMemoryLeak();

		uint64_t GetMemUsage() { return memUsage; }
		uint64_t GetMaxMemUsage() { return maxMemUsage; }

		static MemoryTracker& Get();

	private:
		MemoryTracker();

		struct AllocNode
		{
			size_t size = 0;
			const char* filename;
			int line = -1;

			AllocNode(size_t size_, const char* filename_, int line_) :
				size(size_),
				filename(filename_),
				line(line_)
			{}
		};
		std::unordered_map<void*, AllocNode> mAllocNodeMap;

		volatile uint64_t memUsage = 0;
		volatile uint64_t maxMemUsage = 0;
		char* memoryLeaksFileName = nullptr;
	};
}

#endif