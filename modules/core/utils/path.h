#pragma once

#include "string.h"
#include "stringID.h"
#include "core\collections\hashMap.h"

namespace VulkanTest
{
	struct VULKAN_TEST_API PathInfo
	{
		explicit PathInfo(const char* path);

		char extension[16];
		char basename[MAX_PATH_LENGTH];
		char dir[MAX_PATH_LENGTH];
	};

	using MaxPathString = StaticString<MAX_PATH_LENGTH>;

	class VULKAN_TEST_API Path
	{
	public:
		static const char* INVALID_PATH;
		static const char* PATH_SLASH;

		static MaxPathString Join(Span<const char> base, Span<const char> path);
		static void	Normalize(const char* path, Span<char> outPath);
		static Span<const char> GetDir(const char* path);
		static Span<const char> GetBaseName(const char* path);
		static Span<const char> GetBaseNameWithExtension(const char* path);
		static Span<const char> GetExtension(Span<const char> path);
		static Span<const char> GetPathWithoutExtension(const char* path);
		static bool HasExtension(const char* path, const char* ext);
		static bool ReplaceExtension(char* path, const char* ext);
		static Path ConvertAbsolutePathToRelative(const Path& base, const Path& path);

	public:
		Path();
		explicit Path(const char* path_);
		void operator=(const char* rhs);

		void Join(const char* path_);
		void Append(const char* str);
		void Append(char* str);
		size_t Length()const;
		const char* c_str()const { return path; }
		StringID GetHash()const { return hash; }
		U64 GetHashValue()const { return hash.GetHashValue(); }
		bool IsEmpty()const { return path[0] == '\0'; }
		PathInfo GetPathInfo();
		Span<const char> ToSpan()const { return Span(path, Length()); }
		bool IsRelative()const;
		
		Path operator+(const char* str);
		Path& operator+=(const char* str);

		bool operator==(const Path& rhs) const;
		bool operator!=(const Path& rhs) const;

		Path& operator/=(const char* str);
		Path& operator/=(char c);

		FORCE_INLINE Path& operator/=(const String& str)
		{
			return operator/=(str.c_str());
		}

		FORCE_INLINE Path& operator/=(const Path& str)
		{
			return operator/=(str.c_str());
		}

		FORCE_INLINE Path operator/(const char* str) const
		{
			return Path(*this) /= str;
		}

		FORCE_INLINE Path operator/(const char c) const
		{
			return Path(*this) /= c;
		}

		FORCE_INLINE Path operator/(const String& str) const
		{
			return Path(*this) /= str;
		} 

		FORCE_INLINE Path operator/(const Path& str) const
		{
			return Path(*this) /= str;
		}

		operator const char* () const {
			return path;
		}

	private:
		char path[MAX_PATH_LENGTH];
		StringID hash;
	};

	class FilePathResolver
	{
	public:
		FilePathResolver() {}
		virtual ~FilePathResolver() {}

		virtual bool ResolvePath(const char* inPath, char* outPath, size_t maxOutPath) = 0;
		virtual bool OriginalPath(const char* inPath, char* outPath, size_t maxOutPath) = 0;
	};

	template<>
	struct HashMapHashFunc<Path>
	{
		static U32 Get(const Path& key)
		{
			const U64 hash = key.GetHashValue();
			return U32(hash ^ (hash >> 32));
		}
	};
}