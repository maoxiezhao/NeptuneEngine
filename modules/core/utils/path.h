#pragma once

#include "string.h"

namespace VulkanTest
{
	struct VULKAN_TEST_API PathInfo
	{
		explicit PathInfo(const char* path);

		char extension[16];
		char basename[MAX_PATH_LENGTH];
		char dir[MAX_PATH_LENGTH];
	};

	class VULKAN_TEST_API Path
	{
	public:
		static const char* INVALID_PATH;

		static void	Normalize(const char* path, Span<char> outPath);
		static Span<const char> GetDir(const char* path);
		static Span<const char> GetBaseName(const char* path);
		static Span<const char> GetExtension(Span<const char> path);
		static bool IsAbsolutePath(const char* path);

	public:
		Path();
		explicit Path(const char* path_);
		void operator=(const char* rhs);

		size_t Length()const;
		const char* c_str()const { return path; }
		unsigned int  GetHash()const { return hash; }
		bool IsEmpty()const { return path[0] == '\0'; }
		
		bool operator==(const Path& rhs) const;
		bool operator!=(const Path& rhs) const;

	private:
		char path[MAX_PATH_LENGTH];
		U32 hash = 0;
	};

	class FilePathResolver
	{
	public:
		FilePathResolver() {}
		virtual ~FilePathResolver() {}

		virtual bool ResolvePath(const char* inPath, char* outPath, size_t maxOutPath) = 0;
		virtual bool OriginalPath(const char* inPath, char* outPath, size_t maxOutPath) = 0;
	};

}