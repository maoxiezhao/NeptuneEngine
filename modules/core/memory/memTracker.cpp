#include "memTracker.h"

#ifdef VULKAN_MEMORY_TRACKER
#include "core\platform\sync.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace VulkanTest
{
	Mutex mutex;
	std::ofstream loggerFile;
	bool mIsExit = false;

	MemoryTracker::MemoryTracker()
	{
	}

	void MemoryTracker::RecordAlloc(void* ptr, size_t size, const char* filename, int line)
	{
		ScopedMutex lock(mutex);
		if (size <= 0) {
			return;
		}

		if (mAllocNodeMap.find(ptr) != mAllocNodeMap.end())
		{
			auto kvp = mAllocNodeMap.find(ptr);
			AllocNode& allocNode = kvp->second;
			std::stringstream os;
			os << (allocNode.filename ? allocNode.filename : "(unknown)");
			os << "(" << allocNode.line << ")";
			os << ": alloc size:" << allocNode.size;
			os << std::endl;
			Logger::Info(os.str().c_str());

			ASSERT_MSG(mAllocNodeMap.find(ptr) == mAllocNodeMap.end(), "The address is already allocated");
		}


		mAllocNodeMap.insert(std::pair(ptr, AllocNode(size, filename, line)));
		memUsage += size;
		maxMemUsage = std::max(maxMemUsage, memUsage);
	}

	void MemoryTracker::RecordRealloc(void* ptr, void* old, size_t size, const char* filename, int line)
	{
		ScopedMutex lock(mutex);
		if (ptr != old)
		{
			auto it = mAllocNodeMap.find(old);
			if (it != mAllocNodeMap.end())
			{
				memUsage -= it->second.size;
				mAllocNodeMap.erase(it);
			}
		}

		auto it = mAllocNodeMap.find(ptr);
		if (it == mAllocNodeMap.end())
		{
			mAllocNodeMap.insert(std::pair(ptr, AllocNode(size, filename, line)));
			memUsage += size;
			maxMemUsage = std::max(maxMemUsage, memUsage);
		}
		else
		{
			size_t oldSize = it->second.size;
			it->second.size = size;
			it->second.filename = filename;
			it->second.line = line;

			memUsage += size - oldSize;
			maxMemUsage = std::max(maxMemUsage, memUsage);
		}
	}

	void MemoryTracker::RecordFree(void* ptr)
	{
		if (ptr == nullptr) {
			return;
		}

		ScopedMutex lock(mutex);
		auto it = mAllocNodeMap.find(ptr);
		if (it == mAllocNodeMap.end())
		{
			Logger::Error("The address is already free");
		}
		else
		{
			memUsage -= it->second.size;
			mAllocNodeMap.erase(it);
		}
	}

	void MemoryTracker::ReportMemoryLeak()
	{
		if (mAllocNodeMap.empty()) {
			return;
		}

		std::stringstream os;
		os << std::endl;
		os << "[Memory] Detected memory leaks !!! " << std::endl;
		os << "[Memory] Leaked memory usage:" << memUsage << std::endl;
		os << "[Memory] Dumping allocations:" << std::endl;

		for (auto kvp : mAllocNodeMap)
		{
			AllocNode& allocNode = kvp.second;
			os << (allocNode.filename ? allocNode.filename : "(unknown)");
			os << "(" << allocNode.line << ")";
			os << ": alloc size:" << allocNode.size;
			os << std::endl;
		}

		std::cout << os.str().c_str() << std::endl;
		Logger::Warning(os.str().c_str());

		if (memoryLeaksFileName != nullptr)
		{
			if (!loggerFile.is_open()) {
				loggerFile.open(memoryLeaksFileName);
			}
			
			loggerFile << os.str();
			loggerFile.close();
		}

		mIsExit = true;
	}

	MemoryTracker& MemoryTracker::Get()
	{
		static MemoryTracker tracker;
		return tracker;
	}
}

#endif