#include "stream.h"
#include "string.h"
#include "core\memory\memory.h"
#include "math\geometry.h"

namespace VulkanTest
{
	IOutputStream& IOutputStream::operator << (const char* str)
	{
		Write(str, StringLength(str));
		return *this;
	}

	IOutputStream& IOutputStream::operator<<(I32 data)
	{
		char temp[20];
		ToCString(data, Span(temp));
		Write(temp, StringLength(temp));
		return *this;
	}

	IOutputStream& IOutputStream::operator<<(I64 data)
	{
		char temp[40];
		ToCString(data, Span(temp));
		Write(temp, StringLength(temp));
		return *this;
	}

	IOutputStream& IOutputStream::operator<<(U32 data)
	{
		char temp[20];
		ToCString(data, Span(temp));
		Write(temp, StringLength(temp));
		return *this;
	}

	IOutputStream& IOutputStream::operator<<(U64 data)
	{
		char temp[40];
		ToCString(data, Span(temp));
		Write(temp, StringLength(temp));
		return *this;
	}

	IOutputStream& IOutputStream::operator<<(F32 data)
	{
		char tmp[30];
		ToCString(data, Span(tmp), 6);
		Write(tmp, StringLength(tmp));
		return *this;
	}

	InputMemoryStream::InputMemoryStream(const void* data_, U64 size_) :
		data((const U8*)data_),
		size(size_),
		pos(0)
	{
	}

	InputMemoryStream::InputMemoryStream(const OutputMemoryStream& blob) :
		data((const U8*)blob.Data()),
		size(blob.Size()),
		pos(0)
	{}

	bool InputMemoryStream::Read(void* buffer_, U64 size_)
	{
		if (pos + size_ > size)
		{
			memset(buffer_, 0, size_);
			return false;
		}

		if (size_ > 0)
			memcpy(buffer_, (char*)data + pos, size_);

		pos += size_;
		return true;
	}

	const char* InputMemoryStream::ReadString()
	{
		const char* ret = (const char*)data + pos;
		const char* end = (const char*)data + size;
		while (pos < size && data[pos]) ++pos;
		ASSERT(pos < size);
		++pos;
		return ret;
	}

	String InputMemoryStream::ReadStringWithLength()
	{
		U16 length = Read<U16>();
		String ret;
		ret.resize(length);
		Read(ret.data(), length * sizeof(char));
		return ret;
	}

	void InputMemoryStream::ReadAABB(AABB* aabb)
	{
		F32x3 min = Read<F32x3>();
		F32x3 max = Read<F32x3>();
		*aabb = AABB(min, max);
	}

	IOutputStream& IOutputStream::operator<<(F64 data)
	{
		char temp[40];
		ToCString(data, Span(temp), 12);
		Write(temp, StringLength(temp));
		return *this;
	}

	void IOutputStream::WriteString(const char* string)
	{
		if (string)
		{
			const I32 size = StringLength(string) + 1;
			Write(string, size);
		}
		else
		{
			Write((char)0);
		}
	}

	void IOutputStream::WriteStringWithLength(const String& string)
	{
		Write((U16)string.size());
		Write(string.data(), string.size());
	}

	void IOutputStream::WriteAABB(const AABB& aabb)
	{
		Write(aabb.min);
		Write(aabb.max);
	}

	OutputMemoryStream::OutputMemoryStream() :
		data(nullptr),
		size(0),
		capacity(0),
		allocated(false)
	{
	}

	OutputMemoryStream::OutputMemoryStream(void* data_, U64 size_) :
		data((U8*)data_),
		size(size_),
		capacity(size_),
		allocated(false)
	{
	}

	OutputMemoryStream::OutputMemoryStream(OutputMemoryStream&& rhs)
	{
		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;
		allocated = rhs.allocated;

		rhs.data = nullptr;
		rhs.capacity = 0;
		rhs.size = 0;
		rhs.allocated = false;
	}

	OutputMemoryStream::OutputMemoryStream(const OutputMemoryStream& rhs)
	{
		size = rhs.size;
		if (rhs.capacity > 0)
		{
			if (rhs.allocated)
			{
				data = (U8*)CJING_MALLOC(rhs.capacity);
				Memory::Memcpy(data, rhs.data, capacity);
				capacity = rhs.capacity;
			}
			else
			{
				data = rhs.data;
				capacity = rhs.capacity;
			}

			allocated = rhs.allocated;
		}
		else
		{
			data = nullptr;
			capacity = 0;
			allocated = false;
		}
	}

	OutputMemoryStream::~OutputMemoryStream()
	{
		if (allocated)
			CJING_SAFE_FREE(data);
	}

	void OutputMemoryStream::operator=(const OutputMemoryStream& rhs)
	{
		if (allocated && data != nullptr)
			CJING_SAFE_FREE(data);

		size = rhs.size;
		allocated = rhs.allocated;
		if (rhs.capacity > 0)
		{
			if (allocated)
			{
				data = (U8*)CJING_MALLOC(rhs.capacity);
				Memory::Memcpy(data, rhs.data, capacity);
				capacity = rhs.capacity;
			}
			else
			{
				data = rhs.data;
				capacity = rhs.capacity;
			}
		}
		else
		{
			data = nullptr;
			capacity = 0;
			allocated = false;
		}
	}

	void OutputMemoryStream::operator=(OutputMemoryStream&& rhs)
	{
		if (data != nullptr)
			CJING_SAFE_FREE(data);

		data = rhs.data;
		size = rhs.size;
		capacity = rhs.capacity;
		allocated = rhs.allocated;

		rhs.data = nullptr;
		rhs.capacity = 0;
		rhs.size = 0;
		rhs.allocated = false;
	}

	bool OutputMemoryStream::Write(const void* buffer, U64 size_)
	{
		if (!size_)
			return true;

		if (size + size_ > capacity)
			Reserve((size + size_) << 1);
		
		memcpy((U8*)data + size, buffer, size_);
		size += size_;
		return true;
	}

	bool OutputMemoryStream::Write(const void* buffer, U64 size_, U64 alignment)
	{
		if (!size_)
			return true;

		U64 alignmentSize = AlignTo(size_, alignment);
		if (size + alignmentSize > capacity)
			Reserve((size + alignmentSize) << 1);

		memcpy((U8*)data + size, buffer, size_);
		size += alignmentSize;
		return true;
	}

	void OutputMemoryStream::Skip(U64 size_)
	{
		if (!size_)
			return;

		if (size + size_ > capacity)
			Reserve((size + size_) << 1);

		size += size_;
	}

	void OutputMemoryStream::Resize(U64 newSize)
	{
		size = newSize;
		if (allocated && size <= capacity)
			return;

		U8* tempData = (U8*)CJING_MALLOC(size);
		memcpy(tempData, data, capacity);
		if (allocated) CJING_SAFE_FREE(data);
		data = tempData;
		capacity = size;
		allocated = true;
	}

	void OutputMemoryStream::Reserve(U64 newSize)
	{
		if (allocated && newSize <= capacity)
			return;

		U8* tempData = (U8*)CJING_MALLOC(newSize);
		memcpy(tempData, data, capacity);
		if (allocated) CJING_SAFE_FREE(data);
		data = tempData;
		capacity = newSize;
		allocated = true;
	}

	void OutputMemoryStream::Allocate(U64 newSize)
	{
		Reserve(size + newSize);
	}

	void OutputMemoryStream::Clear()
	{
		size = 0;
	}

	void OutputMemoryStream::Free()
	{
		if (allocated)
			CJING_SAFE_DELETE(data);
		size = 0;
		capacity = 0;
		data = nullptr;
		allocated = false;
	}

	void OutputMemoryStream::Link(const U8* buffer, U64 size_)
	{
		Free();
		allocated = false;
		data = (U8*)buffer;
		size = size_;
		capacity = size_;
	}

	void OutputMemoryStream::Unlink()
	{
		allocated = false;
		data = nullptr;
		size = 0;
		capacity = 0;
	}
}