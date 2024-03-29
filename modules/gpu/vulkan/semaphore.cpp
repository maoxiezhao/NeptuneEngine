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

Semaphore::Semaphore(DeviceVulkan& device_, VkSemaphore semaphore_, bool isSignalled) :
	device(device_),
	semaphore(semaphore_),
	signalled(isSignalled),
	semaphoreType(VK_SEMAPHORE_TYPE_BINARY_KHR)
{
}

Semaphore::Semaphore(DeviceVulkan& device_, U64 timeline_, VkSemaphore semaphore_) :
	device(device_),
	timeline(timeline_),
	semaphore(semaphore_),
	semaphoreType(VK_SEMAPHORE_TYPE_TIMELINE_KHR)
{
	ASSERT(timeline > 0);
}

Semaphore::~Semaphore()
{
	Recycle();
}

void Semaphore::Recycle()
{
	if (semaphore && timeline == 0)
	{
		if (internalSync)
		{
			if (signalled)
				device.ReleaseSemaphoreNolock(semaphore);
			else
				device.RecycleSemaphoreNolock(semaphore);
		}
		else
		{
			if (signalled)
				device.ReleaseSemaphore(semaphore);
			else
				device.RecycleSemaphore(semaphore);
		}
	}
}

Semaphore& Semaphore::operator=(Semaphore&& other) noexcept
{
	if (this == &other)
		return *this;

	ASSERT(&device == &other.device);
	Recycle();

	semaphore = other.semaphore;
	timeline = other.timeline;
	signalled = other.signalled;
	semaphoreType = other.semaphoreType;
	isPendingWait = other.isPendingWait;

	other.semaphore = VK_NULL_HANDLE;
	other.timeline = 0;
	other.signalled = false;
	other.timeline = 0;
	other.isPendingWait = false;

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
	if (vkCreateSemaphore(device->device, &info, nullptr, &ret) != VK_SUCCESS)
	{
		Logger::Error("Failed to create semaphore");
		ret = VK_NULL_HANDLE;
	}
	return ret;
}

void SemaphoreManager::Recyle(VkSemaphore semaphore)
{
	seamphores.push_back(semaphore);
}

}
}