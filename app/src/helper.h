#pragma once

#include <string>
#include <vector>

namespace Helper
{
	bool FileRead(const std::string& fileName, std::vector<uint8_t>& data);
	bool FileWrite(const std::string& fileName, const uint8_t* data, size_t size);
	bool FileExists(const std::string& fileName);
}