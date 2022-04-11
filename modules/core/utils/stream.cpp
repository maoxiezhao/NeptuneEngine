#include "stream.h"
#include "string.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	OutputMemoryStream::OutputMemoryStream() :
		data(nullptr),
		size(0),
		capacity(0)
	{
	}

	OutputMemoryStream::OutputMemoryStream(void* data_, U64 size_) :
		data((U8*)data_),
		size(0),
		capacity(size_)
	{
	}

	OutputMemoryStream::OutputMemoryStream(OutputMemoryStream&& rhs)
	{
		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;

		rhs.data = nullptr;
		rhs.capacity = 0;
		rhs.size = 0;
	}

	OutputMemoryStream::OutputMemoryStream(const OutputMemoryStream& rhs)
	{
		size = rhs.size;
		if (rhs.capacity > 0)
		{
			data = (U8*)CJING_MALLOC(rhs.capacity);
			Memory::Memcpy(data, rhs.data, capacity);
			capacity = rhs.capacity;
		}
		else
		{
			data = nullptr;
			capacity = 0;
		}
	}

	OutputMemoryStream::~OutputMemoryStream()
	{
		CJING_SAFE_FREE(data);
	}

	void OutputMemoryStream::operator=(const OutputMemoryStream& rhs)
	{
		if (data != nullptr)
			CJING_SAFE_FREE(data);

		size = rhs.size;
		if (rhs.capacity > 0)
		{
			data = (U8*)CJING_MALLOC(rhs.capacity);
			Memory::Memcpy(data, rhs.data, capacity);
			capacity = rhs.capacity;
		}
		else
		{
			data = nullptr;
			capacity = 0;
		}
	}

	void OutputMemoryStream::operator=(OutputMemoryStream&& rhs)
	{
		if (data != nullptr)
			CJING_SAFE_FREE(data);

		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;

		rhs.data = nullptr;
		rhs.capacity = 0;
		rhs.size = 0;
	}

	bool OutputMemoryStream::Write(const void* buffer, U64 size_)
	{
		if (!size) 
			return true;

		if (size + size_ > capacity)
			Reserve((size + size_) << 1);
		
		memcpy((U8*)data + size, data, size_);
		size += size_;
		return true;
	}

	void OutputMemoryStream::Resize(U64 newSize)
	{
		size = newSize;
		if (size <= capacity) 
			return;

		U8* tempData = (U8*)CJING_MALLOC(size);
		memcpy(tempData, data, capacity);
		CJING_SAFE_FREE(data);
		data = tempData;
		capacity = size;
	}

	void OutputMemoryStream::Reserve(U64 newSize)
	{
		if (newSize <= capacity)
			return;

		U8* tempData = (U8*)CJING_MALLOC(newSize);
		memcpy(tempData, data, capacity);
		CJING_SAFE_FREE(data);
		data = tempData;
		capacity = newSize;
	}

	void OutputMemoryStream::Clear()
	{
		size = 0;
	}

	void OutputMemoryStream::Free()
	{
		CJING_SAFE_DELETE(data);
		size = 0;
		capacity = 0;
		data = nullptr;
	}
}