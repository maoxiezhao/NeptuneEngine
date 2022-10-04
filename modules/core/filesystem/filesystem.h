#pragma once

#include "core\common.h"
#include "core\memory\memory.h"
#include "core\platform\file.h"
#include "core\platform\platform.h"
#include "core\utils\string.h"
#include "core\utils\path.h"
#include "core\utils\delegate.h"
#include "core\serialization\stream.h"

namespace VulkanTest
{
	enum class EnumrateMode
	{
		File = 1 << 0,
		Directory = 1 << 1,
		All = File | Directory
	};

	class VULKAN_TEST_API FileSystem
	{
	public:
		static bool MoveFile(const char* from, const char* to);
		static bool CopyFile(const char* from, const char* to);
		static bool DeleteFile(const char* path);
		static bool FileExists(const char* path);
		static UniquePtr<File>  OpenFile(const char* path, FileFlags flags);
		static bool StatFile(const char* path, FileInfo& stat);
		static U64  GetLastModTime(const char* path);
		static std::vector<ListEntry> Enumerate(const char* path, int mask = (int)EnumrateMode::All);
		static bool LoadContext(const char* path, OutputMemoryStream& mem);
	};
}