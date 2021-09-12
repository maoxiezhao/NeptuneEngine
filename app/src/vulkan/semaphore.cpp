#include "semaphore.h"
#include "vulkan/device.h"

void SemaphoreDeleter::operator()(Semaphore* semaphore)
{
	semaphore->mDevice.mSemaphorePool.free(semaphore);
}

Semaphore::Semaphore(DeviceVulkan& device, VkSemaphore semaphore, bool isSignalled) :
	mDevice(device),
	mSemaphore(semaphore),
	mSignalled(isSignalled)
{
}

Semaphore::~Semaphore()
{
	if (mSemaphore != VK_NULL_HANDLE)
	{
		mDevice.ReleaseSemaphore(mSemaphore, mSignalled);
	}
}

SemaphoreManager::~SemaphoreManager()
{
}

void SemaphoreManager::Initialize(DeviceVulkan& device)
{
	mDevice = &device;
}

VkSemaphore SemaphoreManager::Requset()
{
	if (!mSeamphores.empty())
	{
		auto ret = mSeamphores.back();
		mSeamphores.pop_back();
		return ret;
	}

	VkSemaphore ret;
	VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	vkCreateSemaphore(mDevice->mDevice, &info, nullptr, &ret);
	return ret;
}

void SemaphoreManager::Recyle(VkSemaphore semaphore)
{
	mSeamphores.push_back(semaphore);
}

void SemaphoreManager::ClearAll()
{
	for (auto& semaphore : mSeamphores)
		vkDestroySemaphore(mDevice->mDevice, semaphore, nullptr);
}
