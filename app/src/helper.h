#pragma once

#include <string>
#include <vector>

namespace Helper
{
	void DirectoryCreate(const std::string& path);
	void MakePathRelative(const std::string& rootdir, std::string& path);
	void MakePathAbsolute(std::string& path);
	std::string GetDirectoryFromPath(const std::string& path);
	std::string ReplaceExtension(const std::string& filename, const std::string& extension);
	bool FileRead(const std::string& fileName, std::vector<uint8_t>& data);
	bool FileWrite(const std::string& fileName, const uint8_t* data, size_t size);
	bool FileExists(const std::string& fileName);
	void StringConvert(const std::string& from, std::wstring& to);
	void StringConvert(const std::wstring& from, std::string& to);
	std::string GetFileNameFromPath(const std::string& fullPath);
}