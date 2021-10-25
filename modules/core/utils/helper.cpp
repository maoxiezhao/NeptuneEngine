#include "helper.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#endif

namespace Helper
{
	void DirectoryCreate(const std::string& path)
	{
		std::filesystem::create_directories(path);
	}

	void MakePathRelative(const std::string& rootdir, std::string& path)
	{
		if (rootdir.empty() || path.empty())
		{
			return;
		}

		std::filesystem::path filepath = path;
		std::filesystem::path rootpath = rootdir;
		std::filesystem::path relative = std::filesystem::relative(path, rootdir);
		if (!relative.empty())
		{
			path = relative.string();
		}
	}

	void MakePathAbsolute(std::string& path)
	{
		std::filesystem::path filepath = path;
		std::filesystem::path absolute = std::filesystem::absolute(path);
		if (!absolute.empty())
			path = absolute.string();
	}

	void SplitPath(const std::string& fullPath, std::string& dir, std::string& fileName)
	{
		size_t found;
		found = fullPath.find_last_of("/\\");
		dir = fullPath.substr(0, found + 1);
		fileName = fullPath.substr(found + 1);
	}

	std::string GetDirectoryFromPath(const std::string& path)
	{
		if (path.empty())
			return path;

		std::string ret, empty;
		SplitPath(path, ret, empty);
		return ret;
	}

	std::string ReplaceExtension(const std::string& filename, const std::string& extension)
	{
		size_t idx = filename.rfind('.');
		if (idx != std::string::npos)
			return filename.substr(0, idx + 1) + extension;

		return filename;
	}

	bool FileRead(const std::string& fileName, std::vector<uint8_t>& data)
	{
		std::ifstream file(fileName, std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			size_t dataSize = (size_t)file.tellg();
			file.seekg(0, file.beg);
			data.resize(dataSize);
			file.read((char*)data.data(), dataSize);
			file.close();
			return true;
		}

		return false;
	}

	bool FileWrite(const std::string& fileName, const uint8_t* data, size_t size)
	{
		if (size <= 0)
			return false;

		std::ofstream file(fileName, std::ios::binary | std::ios::trunc);
		if (file.is_open())
		{
			file.write((const char*)data, (std::streamsize)size);
			file.close();
			return true;
		}

		return false;
	}

	bool FileExists(const std::string& fileName)
	{
		return std::filesystem::exists(fileName);
	}

	void StringConvert(const std::string& from, std::wstring& to)
	{
#ifdef _WIN32
		int num = MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, NULL, 0);
		if (num > 0)
		{
			to.resize(size_t(num) - 1);
			MultiByteToWideChar(CP_UTF8, 0, from.c_str(), -1, &to[0], num);
		}
#else
		to = std::wstring(from.begin(), from.end()); // TODO
#endif // _WIN32
	}

	void StringConvert(const std::wstring& from, std::string& to)
	{
#ifdef _WIN32
		int num = WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, NULL, 0, NULL, NULL);
		if (num > 0)
		{
			to.resize(size_t(num) - 1);
			WideCharToMultiByte(CP_UTF8, 0, from.c_str(), -1, &to[0], num, NULL, NULL);
		}
#else
		to = std::string(from.begin(), from.end()); // TODO
#endif // _WIN32
	}

	std::string GetFileNameFromPath(const std::string& fullPath)
	{
		if (fullPath.empty())
		{
			return fullPath;
		}

		std::string ret, empty;
		SplitPath(fullPath, empty, ret);
		return ret;
	}
}