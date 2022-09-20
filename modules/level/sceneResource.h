#pragma once

#include "core\common.h"
#include "content\jsonResource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API SceneResource final : public JsonResource
	{
	public:
		DECLARE_RESOURCE(SceneResource);

		SceneResource(const ResourceInfo& info);
		virtual ~SceneResource();
	};
}