#include "descriptorSet.h"

namespace GPU
{
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

	std::pair<VkDescriptorSet, bool> DescriptorSetAllocator::FindSet(HashValue hash)
	{
		return std::pair<VkDescriptorSet, bool>();
	}
}