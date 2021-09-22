#pragma once

#include "definition.h"

struct DeviceFeatures
{
    bool SupportsSurfaceCapabilities2 = false;
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

    bool Initialize(std::vector <const char*> instanceExt, bool debugLayer);

    VkDevice GetDevice()const
    {
        return mDevice;
    }

    VkInstance GetInstance()const
    {
        return mInstance;
    }

private:
    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak);
    
    // debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        Logger::Error("validation layer: %s", pCallbackData->pMessage);
        return VK_FALSE;
    }

private:
    friend class DeviceVulkan;

    // base info
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    // core 
    VkDevice mDevice;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;

    // features
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};
    DeviceFeatures mExtensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};

    // queue
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    QueueInfo mQueueInfo = {};
};