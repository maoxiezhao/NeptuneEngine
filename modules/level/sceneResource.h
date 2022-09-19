#pragma once

#include "core\common.h"
#include "content\binaryResource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API SceneResource final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(SceneResource);

		SceneResource(const ResourceInfo& info, ResourceManager& resManager);
		virtual ~SceneResource();

	protected:
		friend class Font;

		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;
	};
}