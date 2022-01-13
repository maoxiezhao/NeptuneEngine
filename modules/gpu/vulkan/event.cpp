#include "semaphore.h"
#include "vulkan/device.h"
#include "event.h"

namespace VulkanTest
{
namespace GPU
{
	void EventDeleter::operator()(Event* ent)
	{
		ent->device.eventPool.free(ent);
	}

	Event::Event(DeviceVulkan& device_, VkEvent ent_) :
		device(device_),
		ent(ent_)
	{
	}

	Event::~Event()
	{
		if (ent != VK_NULL_HANDLE)
		{
			device.ReleaseEvent(ent);
		}
	}

	EventManager::~EventManager()
	{
		for (auto& ent : events)
			vkDestroyEvent(device->device, ent, nullptr);
	}

	void EventManager::Initialize(DeviceVulkan& device_)
	{
		device = &device_;
	}

	VkEvent EventManager::Requset()
	{
		if (!events.empty())
		{
			auto ent = events.back();
			events.pop_back();
			return ent;
		}

		VkEvent ent = {};
		VkEventCreateInfo info = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
		vkCreateEvent(device->device, &info, nullptr, &ent);
		return ent;
	}

	void EventManager::Recyle(VkEvent ent)
	{
		if (ent != VK_NULL_HANDLE)
		{
			vkResetEvent(device->device, ent);
			events.push_back(ent);
		}
	}
}
}