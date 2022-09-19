#pragma once

#include "core\common.h"
#include "jsonFwd.h"

namespace VulkanTest
{
	class JsonWriteStream;

	class VULKAN_TEST_API ISerializable
	{
	public:
		using SerializeDocument = rapidjson_flax::Document;
		using SerializeStream = JsonWriteStream;
		using DeserializeStream = rapidjson_flax::Value;

		virtual ~ISerializable() = default;

		virtual void Serialize(SerializeStream& stream, const void* otherObj) = 0;
		virtual void Deserialize(DeserializeStream& stream) = 0;
	};
}