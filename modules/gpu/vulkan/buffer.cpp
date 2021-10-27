#include "buffer.h"

namespace GPU
{
	void BufferDeleter::operator()(Buffer* buffer)
	{
	}

	Buffer::~Buffer()
	{
	}
}