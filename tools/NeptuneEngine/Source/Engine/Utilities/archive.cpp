#include "archive.h"
#include "helper.h"

#include <fstream>
#include <iostream>

namespace VulkanTest
{
// this should always be only INCREMENTED and only if a new serialization is implemeted somewhere!
uint64_t __archiveVersion = 72;
// this is the version number of which below the archive is not compatible with the current version
uint64_t __archiveVersionBarrier = 22;

Archive::Archive()
{
	CreateEmpty();
}
Archive::Archive(const std::string& fileName, bool readMode) : fileName(fileName), readMode(readMode)
{
	if (!fileName.empty())
	{
		directory = Helper::GetDirectoryFromPath(fileName);
		if (readMode)
		{
			if (Helper::FileRead(fileName, DATA))
			{
				(*this) >> version;
				if (version < __archiveVersionBarrier)
				{
					std::string ss = "The archive version (" + std::to_string(version) + ") is no longer supported!";
					std::cout << ss << std::endl;
					Close();
				}
				if (version > __archiveVersion)
				{
					std::string ss = "The archive version (" + std::to_string(version) + ") is higher than the program's (" + std::to_string(__archiveVersion) + ")!";
					std::cout << ss << std::endl;
					Close();
				}
			}
		}
		else
		{
			CreateEmpty();
		}
	}
}

void Archive::CreateEmpty()
{
	readMode = false;
	pos = 0;

	version = __archiveVersion;
	DATA.resize(128); // starting size
	(*this) << version;
}

void Archive::SetReadModeAndResetPos(bool isReadMode)
{
	readMode = isReadMode;
	pos = 0;

	if (readMode)
	{
		(*this) >> version;
	}
	else
	{
		(*this) << version;
	}
}

bool Archive::IsOpen()
{
	// when it is open, DATA is not null because it contains the version number at least!
	return !DATA.empty();
}

void Archive::Close()
{
	if (!readMode && !fileName.empty())
	{
		SaveFile(fileName);
	}
	DATA.clear();
}

bool Archive::SaveFile(const std::string& fileName)
{
	return Helper::FileWrite(fileName, DATA.data(), pos);
}

const std::string& Archive::GetSourceDirectory() const
{
	return directory;
}

const std::string& Archive::GetSourceFileName() const
{
	return fileName;
}
}