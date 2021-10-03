#include "descriptorSet.h"

namespace GPU
{
	static const unsigned VULKAN_NUM_SETS_PER_POOL = 16;

	DescriptorSetAllocator::DescriptorSetAllocator(DeviceVulkan& device_) :
		device(device_)
	{
	}

	DescriptorSetAllocator::~DescriptorSetAllocator()
	{
	}

	void DescriptorSetAllocator::BeginFrame()
	{
	}

	std::pair<VkDescriptorSet, bool> DescriptorSetAllocator::GetOrAllocate(HashValue hash)
	{
		auto it = setMap.find(hash);
		if (it != setMap.end())
			return { *it->second, true };
		
		// create descriptor pool
		VkDescriptorPool pool;
		VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		info.maxSets = VULKAN_NUM_SETS_PER_POOL;
		if (!poolSize.empty())
		{
			info.poolSizeCount = poolSize.size();
			info.pPoolSizes = poolSize.data();
		}

		if (vkCreateDescriptorPool(device.device, &info, nullptr, &pool) != VK_SUCCESS)
		{
			return { VK_NULL_HANDLE, false };
		}

		// Create descriptor set
		// 一次性分配VULKAN_NUM_SETS_PER_POOL个descriptor set并缓存起来，以减少分配的次数
		VkDescriptorSet sets[VULKAN_NUM_SETS_PER_POOL];
		VkDescriptorSetLayout layouts[VULKAN_NUM_SETS_PER_POOL];
		std::fill(begin(layouts), end(layouts), setLayout);

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = VULKAN_NUM_SETS_PER_POOL;
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(device.device, &alloc, sets) != VK_SUCCESS)
		{
			return { VK_NULL_HANDLE, false };
		}

		// 缓存sets，并分配一个set返回

		return std::pair<VkDescriptorSet, bool>();
	}
}