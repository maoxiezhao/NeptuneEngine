#pragma once

#include "json.h"
#include "stream.h"

namespace VulkanTest
{
	class VULKAN_TEST_API JsonWriteStream final : public IOutputStream
	{
		using IOutputStream::Write;

		JsonWriteStream();
		~JsonWriteStream();

		JsonWriteStream(const JsonWriteStream& rhs) = delete;
		void operator=(const JsonWriteStream& rhs) = delete;

		bool Write(const void* data, U64 size) override;
	};
}