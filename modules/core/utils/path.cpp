#include "path.h"
#include "core\collections\array.h"

namespace VulkanTest
{
	const char* Path::INVALID_PATH = "N/A";

#ifdef CJING3D_PLATFORM_WIN32
	const char* Path::PATH_SLASH = "/\\";
#else
	const char* Path::PATH_SLASH = "/";
#endif

	StaticString<MAX_PATH_LENGTH> Path::Join(Span<const char> base, Span<const char> path)
	{
		if (base.empty())
			return path.data();
		if (path.empty())
			return base.data();

		StaticString<MAX_PATH_LENGTH> ret(base.data());
		int index = FindStringChar(base.data(), *Path::PATH_SLASH, 0);
		if (index != base.length() - 1)
			ret.append(Path::PATH_SLASH);

		ret.append(path.data());
		return ret;
	}

	void Path::Normalize(const char* path, Span<char> outPath)
	{
		if (path == nullptr) {
			return;
		}

		char* out = outPath.begin();
		U32 maxLength = (U32)outPath.length();
		U32 i = 0;
		bool isPrevSlash = false;

		if (path[0] == '.' && (path[1] == '\\' || path[1] == '/')) {
			path += 2;
		}
#ifdef CJING3D_PLATFORM_WIN32
		if (path[0] == '\\' || path[0] == '/') {
			++path;
		}
#endif
		while (*path != '\0' && i < maxLength)
		{
			bool isCurrentSlash = *path == '\\' || *path == '/';
			if (isCurrentSlash && isPrevSlash)
			{
				++path;
				continue;
			}
			*out = *path == '\\' ? '/' : *path;

			path++;
			out++;
			i++;
			isPrevSlash = isCurrentSlash;
		}
		(i < maxLength ? *out : *(out - 1)) = '\0';
	}

	Span<const char> Path::GetDir(const char* path)
	{
		if (!path[0]) return { nullptr, nullptr };

		Span<const char> dir;
		dir.pBegin = path;
		dir.pEnd = path + StringLength(path) - 1;
		while (dir.pEnd != dir.pBegin && *dir.pEnd != '\\' && *dir.pEnd != '/')
			--dir.pEnd;
		if (dir.pEnd != dir.pBegin) 
			++dir.pEnd;
		return dir;
	}

	Span<const char> Path::GetBaseName(const char* path)
	{
		if (!path[0]) return { nullptr, nullptr };

		Span<const char> basename;
		const char* end = path + StringLength(path);
		basename.pEnd = end;
		basename.pBegin = end;
		while (basename.pBegin != path && *basename.pBegin != '\\' && *basename.pBegin != '/')
			--basename.pBegin;
		if (*basename.pBegin == '\\' || *basename.pBegin == '/') 
			++basename.pBegin;

		basename.pEnd = basename.pBegin;
		while (basename.pEnd != end && *basename.pEnd != '.') ++basename.pEnd;
		return basename;
	}

	Span<const char> Path::GetExtension(Span<const char> path)
	{
		if (path.length() <= 0) return { nullptr, nullptr };

		Span<const char> ext;
		ext.pEnd = path.pEnd;
		ext.pBegin = path.pEnd - 1;
		while (ext.pBegin != path.pBegin && *ext.pBegin != '.') {
			--ext.pBegin;
		}
		if (*ext.pBegin != '.')  return { nullptr, nullptr };
		ext.pBegin++;
		return ext;
	}

	Span<const char> Path::GetPathWithoutExtension(const char* path)
	{
		if (!path[0]) return { nullptr, nullptr };

		Span<const char> dir;
		dir.pBegin = path;
		dir.pEnd = path + StringLength(path) - 1;
		while (dir.pEnd != dir.pBegin && *dir.pEnd != '.')
			--dir.pEnd;
		return dir;
	}

	bool Path::HasExtension(const char* path, const char* ext)
	{
		char tmp[20];
		CopyString(Span(tmp), GetExtension(Span(path, StringLength(path))));
		return EqualIStrings(tmp, ext);
	}

	bool Path::ReplaceExtension(char* path, const char* ext)
	{
		char* end = path + StringLength(path);
		while (end > path && *end != '.')
			--end;
		if (*end != '.') 
			return false;

		++end;
		const char* src = ext;
		while (*src != '\0' && *end != '\0')
		{
			*end = *src;
			++end;
			++src;
		}
		bool copied_whole_ext = *src == '\0';
		if (!copied_whole_ext) 
			return false;

		*end = '\0';
		return true;
	}

	static void SplitPath(const String& path, Array<String>& splitPath)
	{
		I32 start = 0;
		I32 separatorPos;
		do
		{
			separatorPos = path.find("/", start);
			if (separatorPos == -1)
				splitPath.push_back(path.substr(start));
			else
				splitPath.push_back(path.substr(start, separatorPos - start));
			start = separatorPos + 1;
		} while (separatorPos != -1);
	}

	Path Path::ConvertAbsolutePathToRelative(const Path& base, const Path& path)
	{
		Array<String> toDirs;
		Array<String> fromDirs;
		SplitPath(path.c_str(), toDirs);
		SplitPath(base.c_str(), fromDirs);

		String output;
		auto toIt = toDirs.begin(), fromIt = fromDirs.begin();
		const auto toEnd = toDirs.end(), fromEnd = fromDirs.end();
		while (toIt != toEnd && fromIt != fromEnd && *toIt == *fromIt)
		{
			++toIt;
			++fromIt;
		}

		while (fromIt != fromEnd)
		{
			output += "../";
			++fromIt;
		}

		while (toIt != toEnd)
		{
			output += *toIt;
			++toIt;

			if (toIt != toEnd)
				output += '/';
		}

		return Path(output);
	}

	PathInfo::PathInfo(const char* path)
	{
		char tempPath[MAX_PATH_LENGTH];
		Path::Normalize(path, tempPath);
		CopyString(Span(extension), Path::GetExtension(Span(tempPath, StringLength(tempPath))));
		CopyString(Span(basename), Path::GetBaseName(path));
		CopyString(Span(dir), Path::GetDir(path));
	}

	Path::Path() :
		hash(0),
		path()
	{
	}

	Path::Path(const char* path_)
	{
		Path::Normalize(path_, path);
		hash = StringID(path);
	}

	void Path::operator=(const char* rhs)
	{
		Path::Normalize(rhs, path);
		hash = StringID(path);
	}

	void Path::Join(const char* path_)
	{
		auto newPath = Path::Join(path, Span(path_, StringLength(path_)));
		Path::Normalize(newPath, path);
		hash = StringID(path);
	}

	void Path::Append(const char* str)
	{
		CatString(path, str);
		hash = StringID(path);
	}

	void Path::Append(char* str)
	{
		CatString(path, str);
		hash = StringID(path);
	}

	size_t Path::Length() const
	{
		return StringLength(path);
	}

	PathInfo Path::GetPathInfo()
	{
		return PathInfo(path);
	}

	bool Path::IsRelative() const
	{
		return (StringLength(path) >= 2 && iswalpha(path[0]) && path[1] == ':') ||
			StartsWith(path, "\\\\") ||
			StartsWith(path, "\\") ||
			StartsWith(path, "/");
	}

	Path VulkanTest::Path::operator+(const char* str)
	{
		Path ret = *this;
		ret.Append(str);
		return ret;
	}

	Path& VulkanTest::Path::operator+=(const char* str)
	{
		Append(str);
		return *this;
	}

	Path& Path::operator/=(const char* str)
	{
		Join(str);
		return *this;
	}

	Path& Path::operator/=(char c)
	{
		Join(String(c).c_str());
		return *this;
	}

	bool Path::operator==(const Path& rhs) const
	{
		return hash == rhs.hash;
	}

	bool Path::operator!=(const Path& rhs) const
	{
		return hash != rhs.hash;
	}

}