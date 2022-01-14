#include "semaphore.h"
#include "vulkan/device.h"

namespace VulkanTest
{
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
	Recycle();
}

void Semaphore::Recycle()
{
	if (semaphore)
	{
		if (signalled)
			device.ReleaseSemaphore(semaphore);
		else
			device.RecycleSemaphore(semaphore);
	}
}

Semaphore& Semaphore::operator=(Semaphore&& other) noexcept
{
	if (this == &other)
		return *this;

	Recycle();

	semaphore = other.semaphore;
	timeline = other.timeline;
	signalled = other.signalled;

	other.semaphore = VK_NULL_HANDLE;
	other.timeline = 0;
	other.signalled = false;

	return *this;
}

SemaphoreManager::~SemaphoreManager()
{
	for (auto& semaphore : seamphores)
		vkDestroySemaphore(device->device, semaphore, nullptr);
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

}
}