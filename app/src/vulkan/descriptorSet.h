#pragma once

#include "definition.h"

namespace GPU
{
	class DescriptorSetAllocator : public HashedObject<DescriptorSetAllocator>
	{
	public:
		DescriptorSetAllocator(DeviceVulkan& device_);
		~DescriptorSetAllocator();

		void BeginFrame();

	private:
		DeviceVulkan& device;
		VkDescriptorSetLayout setLayout;
	};
}