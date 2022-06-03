#include "fence.h"
#include "vulkan/device.h"

namespace VulkanTest
{
namespace GPU
{

Fence::Fence(DeviceVulkan& device_, VkFence fence) :
	device(device_),
	fence(fence),
	timeline(0),
	timelineSemaphore(VK_NULL_HANDLE)
{
}

Fence::Fence(DeviceVulkan& device_, U64 timeline_, VkSemaphore timelineSemaphore_) :
	device(device_),
	fence(VK_NULL_HANDLE),
	timeline(timeline_),
	timelineSemaphore(timelineSemaphore_)
{
	ASSERT(timeline > 0);
}

Fence::~Fence()
{
	if (fence != VK_NULL_HANDLE)
		device.ReleaseFence(fence, isWaiting);
}

void Fence::Wait()
{
	std::lock_guard<std::mutex> holder{ lock };

	if (isWaiting)
		return;

	if (timeline != 0)
	{
		ASSERT(timelineSemaphore);
		VkSemaphoreWaitInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		info.semaphoreCount = 1;
		info.pSemaphores = &timelineSemaphore;
		info.pValues = &timeline;
	
		if (vkWaitSemaphores(device.device, &info, UINT64_MAX) != VK_SUCCESS)
			Logger::Error("Failed to wait for timeline semaphore!");
		else
			isWaiting = true;
	}
	else
	{
		if (vkWaitForFences(device.device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
			Logger::Error("Failed to wait for fence!");
		else
			isWaiting = true;
	}
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
	for (auto& fence : fences)
		vkDestroyFence(device->device, fence, nullptr);
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

}
}