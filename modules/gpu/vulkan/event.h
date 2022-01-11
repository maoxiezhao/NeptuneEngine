#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{
class DeviceVulkan;
class Event;

struct EventDeleter
{
    void operator()(Event* ent);
};
class Event : public Util::IntrusivePtrEnabled<Event, EventDeleter>
{
public:
    Event(DeviceVulkan& device_, VkEvent ent_);
    ~Event();

private:
    friend class DeviceVulkan;
    friend struct EventDeleter;
    friend class Util::ObjectPool<Event>;

    DeviceVulkan& device;
    VkEvent ent;
};
using EventPtr = Util::IntrusivePtr<Event>;

class EventManager
{
public:
    ~EventManager();

    void Initialize(DeviceVulkan& device_);
    VkEvent Requset();
    void Recyle(VkEvent ent);

private:
    DeviceVulkan* device = nullptr;
    std::vector<VkEvent> events;
};

}
}