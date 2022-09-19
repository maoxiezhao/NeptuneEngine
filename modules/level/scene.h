#pragma once

#include "core\common.h"
#include "core\serialization\iSerializable.h"
#include "sceneResource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Scene : public ISerializable
	{
	public:

		void Serialize(SerializeStream& stream, const void* otherObj) override;
		void Deserialize(DeserializeStream& stream) override;
	};
}