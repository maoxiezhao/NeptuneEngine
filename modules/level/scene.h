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
		Scene();
		Scene(const ScriptingObjectParams& params);
		~Scene();

		void Start();
		void Stop();
		bool IsPlaying()const {
			return isPlaying;
		}

		void Serialize(SerializeStream& stream, const void* otherObj) override;
		void Deserialize(DeserializeStream& stream) override;

		World* GetWorld() {
			return world;
		}
		const World* GetWorld()const {
			return world;
		}

		String GetName()const {
			return name;
		}

		void SetName(const char* name_) {
			name = name_;
		}

	private:
		World* world;
		String name;
		bool isPlaying = false;
	};
}