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
}