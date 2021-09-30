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

		VkDescriptorSetLayout GetSetLayout()
		{
			return setLayout;
		}

		std::pair<VkDescriptorSet, bool> FindSet(HashValue hash);

	private:
		DeviceVulkan& device;
		VkDescriptorSetLayout setLayout;
		std::vector<VkDescriptorPool> pools;
		std::vector<VkDescriptorPoolSize> poolSize;
		bool isBindLess = false;
	};
}