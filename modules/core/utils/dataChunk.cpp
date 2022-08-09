#include "dataChunk.h"
#include "core\platform\timer.h"

namespace VulkanTest
{
	void DataChunk::RegisterUsage()
	{
		LastAccessTime = Timer::GetRawTimestamp();
	}
}