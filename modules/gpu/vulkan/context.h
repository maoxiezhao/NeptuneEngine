#pragma once

#include "definition.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{
namespace GPU
{

struct DeviceFeatures
{
    bool supportDebugUtils = false;
    bool supportsPhysicalDeviceProperties2 = false;
    bool supportsSurfaceCapabilities2 = false;
    bool supportDevieDiagnosticCheckpoints = false;
    bool supportsDepthClip = false;
    bool supportConditionRendering = false;

    VkPhysicalDeviceFeatures2 features2 = {};
    VkPhysicalDeviceVulkan11Features features_1_1 = {};
    VkPhysicalDeviceVulkan12Features features_1_2 = {};
    VkPhysicalDeviceProperties2 properties2 = {};
    VkPhysicalDeviceVulkan11Properties properties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties properties_1_2 = {};
    VkPhysicalDeviceDepthClipEnableFeaturesEXT depth_clip_enable_features = {};
    VkPhysicalDeviceConditionalRenderingFeaturesEXT conditional_rendering_features = {};
};

struct QueueInfo
{
    int familyIndices[QUEUE_INDEX_COUNT] = {};
    VkQueue queues[QUEUE_INDEX_COUNT] = {};
};

struct SystemHandles
{
    FileSystem* fileSystem;
};

class VulkanContext
{
public:
    VulkanContext(U32 numThreads_);
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    void operator=(const VulkanContext&) = delete;

    bool Initialize(std::vector<const char*> instanceExt_, std::vector<const char*> deviceExt_, bool debugLayer_);

    // Load vulkan library
    static bool InitLoader(PFN_vkGetInstanceProcAddr addr);

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

    void SetSystemHandles(SystemHandles& handles_)
    {
        handles = handles_;
    }

    SystemHandles& GetSystemHandles()
    {
        return handles;
    }

private:
    bool CreateInstance(std::vector<const char*> instanceExt);
    bool CreateDevice(VkPhysicalDevice physicalDevice_, std::vector<const char*> deviceExt);

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

    uint32_t width = 0;
    uint32_t height = 0;
    bool debugLayer = false;
    SystemHandles handles;
    U32 numThreads = 1;

    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDevcieProps = {};
    VkPhysicalDeviceMemoryProperties physicalDeviceMemProps = {};

    DeviceFeatures ext = {};

    std::vector<VkQueueFamilyProperties> queueFamilies;
    QueueInfo queueInfo = {};
};

}
}