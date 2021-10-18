#pragma once

#include "definition.h"

namespace GPU
{

struct DeviceFeatures
{
    bool supportDebugUtils = false;
    bool supportsSurfaceCapabilities2 = false;
    bool supportsVulkan11Instance = false;
    bool supportsVulkan11Device = false;
};

struct QueueInfo
{
    int mQueueIndices[QUEUE_INDEX_COUNT] = {};
    VkQueue mQueues[QUEUE_INDEX_COUNT] = {};
};

class VulkanContext
{
public:
    VulkanContext() = default;
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    void operator=(const VulkanContext&) = delete;

    bool Initialize(std::vector<const char*> instanceExt_, std::vector<const char*> deviceExt_, bool debugLayer_);

    VkDevice GetDevice()const
    {
        return device;
    }

    VkInstance GetInstance()const
    {
        return instance;
    }

private:
    bool CreateInstance(std::vector<const char*> instanceExt);
    bool CreateDevice(VkPhysicalDevice physicalDevice_, std::vector<const char*> deviceExt, std::vector<const char*> deviceLayers);

    VkApplicationInfo GetApplicationInfo();
    
#ifdef VULKAN_DEBUG
    // debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
#endif

private:
    friend class DeviceVulkan;

    // base info
    uint32_t width = 0;
    uint32_t height = 0;
    bool debugLayer = false;

    // core 
    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDevcieProps = {};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemProps = {};

    // features
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};
    DeviceFeatures extensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};

    // queue
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    QueueInfo queueInfo = {};
};

}