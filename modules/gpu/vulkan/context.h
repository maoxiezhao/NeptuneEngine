#pragma once

#include "definition.h"

namespace VulkanTest
{

class FileSystem;

namespace GPU
{

struct DeviceFeatures
{
    bool supportDebugUtils = false;
    bool supportsVulkan12Instance = false;
    bool supportsVulkan12Device = false;
    bool supportsPhysicalDeviceProperties2 = false;
    bool supportsSurfaceCapabilities2 = false;

    VkPhysicalDeviceFeatures2 features2 = {};
    VkPhysicalDeviceVulkan11Features features_1_1 = {};
    VkPhysicalDeviceVulkan12Features features_1_2 = {};
    VkPhysicalDeviceProperties2 properties2 = {};
    VkPhysicalDeviceVulkan11Properties properties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties properties_1_2 = {};
};

struct QueueInfo
{
    int familyIndices[QUEUE_INDEX_COUNT] = {};
    VkQueue queues[QUEUE_INDEX_COUNT] = {};
};

class VulkanContext
{
public:
    VulkanContext(U32 numThreads_);
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    void operator=(const VulkanContext&) = delete;

    bool Initialize(std::vector<const char*> instanceExt_, std::vector<const char*> deviceExt_, bool debugLayer_);

    VkDevice GetDevice()const
    {
        return device;
    }

    VkPhysicalDevice GetPhysicalDevice()const
    {
        return physicalDevice;
    }

    VkInstance GetInstance()const
    {
        return instance;
    }

    const QueueInfo& GetQueueInfo()const
    {
        return queueInfo;
    }

    struct SystemHandles
    {
        FileSystem* fileSystem;
    };

    void SetSystemHandles(const SystemHandles& handles_)
    {
        handles = handles_;
    }

    const SystemHandles& GetSystemHandles()const
    {
        return handles;
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
    SystemHandles handles;
    U32 numThreads = 1;

    // core 
    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDevcieProps = {};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemProps = {};

    // features
    DeviceFeatures ext = {};

    // queue
    std::vector<VkQueueFamilyProperties> queueFamilyProps;
    std::vector<VkQueueFamilyProperties2> queueFamilyProps2;
    QueueInfo queueInfo = {};
};

}
}