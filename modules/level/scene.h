#pragma once

#include "core\common.h"
#include "core\serialization\iSerializable.h"
#include "sceneResource.h"
#include "core\scene\world.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Scene : public ScriptingObject, public ISerializable
	{
	public:
		Scene(World& world);
		Scene(World& world, const ScriptingObjectParams& params);

		void Serialize(SerializeStream& stream, const void* otherObj) override;
		void Deserialize(DeserializeStream& stream) override;

		World& GetWorld()const {
			return world;
		}

	private:
		World& world;
	};
}