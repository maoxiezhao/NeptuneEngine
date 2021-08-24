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
}