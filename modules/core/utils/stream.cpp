#include "stream.h"
#include "string.h"

namespace VulkanTest
{
	OutputMemoryStream::OutputMemoryStream()
	{
	}
	OutputMemoryStream::OutputMemoryStream(void* data_, U64 size_)
	{
	}

	OutputMemoryStream::OutputMemoryStream(OutputMemoryStream&& rhs)
	{
	}

	OutputMemoryStream::OutputMemoryStream(const OutputMemoryStream& rhs)
	{
	}

	OutputMemoryStream::~OutputMemoryStream()
	{
	}

	void OutputMemoryStream::operator=(const OutputMemoryStream& rhs)
	{
	}

	void OutputMemoryStream::operator=(OutputMemoryStream&& rhs)
	{
	}

	bool OutputMemoryStream::Write(const void* buffer, U64 size)
	{
		return false;
	}

	void OutputMemoryStream::Resize(U64 size_)
	{
	}

	void OutputMemoryStream::Reserve(U64 size_)
	{
	}

	void OutputMemoryStream::Clear()
	{
	}

	void OutputMemoryStream::Free()
	{
	}
}