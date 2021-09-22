#include "fence.h"
#include "vulkan/device.h"

Fence::Fence(DeviceVulkan& device_, VkFence fence) :
	device(device_),
	fence(fence)
{
}

Fence::~Fence()
{
	if (fence != VK_NULL_HANDLE)
		device.ReleaseFence(fence, isWait);
}

void Fence::Wait()
{
	if (isWait)
		return;

	VkResult res = vkWaitForFences(device.device, 1, &fence, VK_TRUE, UINT64_MAX);
	assert(res == VK_SUCCESS);
	isWait = true;
}

void FenceDeleter::operator()(Fence* fence)
{
	fence->device.fencePool.free(fence);
}

FenceManager::FenceManager() 
{
}

FenceManager::~FenceManager()
{
}

void FenceManager::Initialize(DeviceVulkan& device_)
{
	device = &device_;
}

VkFence FenceManager::Requset()
{
	if (!fences.empty())
	{
		auto ret = fences.back();
		fences.pop_back();
		return ret;
	}

	VkFence ret;
	VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(device->device, &info, nullptr, &ret);
	return ret;
}

void FenceManager::Recyle(VkFence fence)
{
	fences.push_back(fence);
}

void FenceManager::ClearAll()
{
	for (auto& fence : fences)
		vkDestroyFence(device->device, fence, nullptr);
}
