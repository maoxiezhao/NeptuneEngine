#include "fence.h"
#include "vulkan/device.h"

Fence::Fence(DeviceVulkan& device, VkFence fence) :
	mDevice(device),
	mFence(fence)
{
}

Fence::~Fence()
{
	if (mFence != VK_NULL_HANDLE)
		mDevice.ReleaseFence(mFence, isWait);
}

void Fence::Wait()
{
	if (isWait)
		return;

	VkResult res = vkWaitForFences(mDevice.mDevice, 1, &mFence, VK_TRUE, UINT64_MAX);
	assert(res == VK_SUCCESS);
	isWait = true;
}

void FenceDeleter::operator()(Fence* fence)
{
	fence->mDevice.mFencePool.free(fence);
}

FenceManager::FenceManager() 
{
}

FenceManager::~FenceManager()
{
}

void FenceManager::Initialize(DeviceVulkan& device)
{
	mDevice = &device;
}

VkFence FenceManager::Requset()
{
	if (!mFences.empty())
	{
		auto ret = mFences.back();
		mFences.pop_back();
		return ret;
	}

	VkFence ret;
	VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(mDevice->mDevice, &info, nullptr, &ret);
	return ret;
}

void FenceManager::Recyle(VkFence fence)
{
	mFences.push_back(fence);
}

void FenceManager::ClearAll()
{
	for (auto& fence : mFences)
		vkDestroyFence(mDevice->mDevice, fence, nullptr);
}
