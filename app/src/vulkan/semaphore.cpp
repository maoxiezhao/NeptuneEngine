#include "semaphore.h"
#include "vulkan/device.h"

namespace GPU
{

void SemaphoreDeleter::operator()(Semaphore* semaphore)
{
	semaphore->device.semaphorePool.free(semaphore);
}

Semaphore::Semaphore(DeviceVulkan& device_, VkSemaphore semaphore, bool isSignalled) :
	device(device_),
	semaphore(semaphore),
	signalled(isSignalled)
{
}

Semaphore::~Semaphore()
{
	if (semaphore != VK_NULL_HANDLE)
	{
		device.ReleaseSemaphore(semaphore, signalled);
	}
}

SemaphoreManager::~SemaphoreManager()
{
}

void SemaphoreManager::Initialize(DeviceVulkan& device_)
{
	device = &device_;
}

VkSemaphore SemaphoreManager::Requset()
{
	if (!seamphores.empty())
	{
		auto ret = seamphores.back();
		seamphores.pop_back();
		return ret;
	}

	VkSemaphore ret;
	VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	vkCreateSemaphore(device->device, &info, nullptr, &ret);
	return ret;
}

void SemaphoreManager::Recyle(VkSemaphore semaphore)
{
	seamphores.push_back(semaphore);
}

void SemaphoreManager::ClearAll()
{
	for (auto& semaphore : seamphores)
		vkDestroySemaphore(device->device, semaphore, nullptr);
}

}