#include "context.h"

#include <set>
#include <array>

namespace VulkanTest
{
namespace GPU
{
    VulkanContext::VulkanContext(U32 numThreads_) :
        numThreads(numThreads_)
    {
    }

    VulkanContext::~VulkanContext()
{
#ifdef VULKAN_DEBUG
    if (mDebugUtilsMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance, mDebugUtilsMessenger, nullptr);
    }
#endif

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

bool VulkanContext::Initialize(std::vector<const char*> instanceExt_, std::vector<const char*> deviceExt_, bool debugLayer_)
{
    debugLayer = debugLayer_;

    VkResult res = volkInitialize();
    if (res != VK_SUCCESS)
    {
        Logger::Error("Faile to initialize volk");
        return false;
    }

    if (!CreateInstance(instanceExt_))
    {
        Logger::Error("Faile to create vulkan instance");
        return false;
    }
  
    if (!CreateDevice(VK_NULL_HANDLE, deviceExt_, {}))
    {
        Logger::Error("Faile to create vulkan device");
        return false;
    }

    return true;
}

bool VulkanContext::CreateInstance(std::vector<const char*> instanceExt)
{
    VkInstanceCreateInfo instCreateInfo = {};
    ext.supportsVulkan12Instance = volkGetInstanceVersion() >= VK_API_VERSION_1_2;

    VkApplicationInfo appInfo = GetApplicationInfo();
    std::vector<const char*> instanceExts;
    std::vector<const char*> instanceLayers;

    // enum available extension
    for (const auto& ext : instanceExt)
        instanceExts.push_back(ext);

    uint32_t extensionCount = 0;
    VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> quiredExtensions(extensionCount);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, quiredExtensions.data());
    assert(res == VK_SUCCESS);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // check extension
    const auto HasExtension = [&](const char* name)
    {
        auto it = std::find_if(std::begin(quiredExtensions), std::end(quiredExtensions), [name](const VkExtensionProperties& e) -> bool {
            return strcmp(e.extensionName, name) == 0;
            });
        return it != quiredExtensions.end();
    };

    for (const auto& ext : instanceExt)
    {
        if (!HasExtension(ext))
            return false;
    }

    if (HasExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
        instanceExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        ext.supportsPhysicalDeviceProperties2 = true;
    }

    if (HasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        ext.supportDebugUtils = true;
    }

    auto itr = std::find_if(instanceExts.begin(), instanceExts.end(), [](const char* name) {
        return strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        });
    bool has_surface_extension = itr != instanceExts.end();
    if (has_surface_extension && HasExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
    {
        instanceExts.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        ext.supportsSurfaceCapabilities2 = true;
    }


#ifdef VULKAN_DEBUG
    // check support valiadation layer
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> queriedLayers(layerCount);
    if (layerCount)
        vkEnumerateInstanceLayerProperties(&layerCount, queriedLayers.data());

    const auto HasLayer = [&](const char* name)
    {
        auto it = std::find_if(std::begin(queriedLayers), std::end(queriedLayers), [name](const VkLayerProperties& e) -> bool {
            return strcmp(e.layerName, name) == 0;
            });
        return it != queriedLayers.end();
    };

    VkValidationFeaturesEXT validationFeatures = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
    if (debugLayer && HasLayer("VK_LAYER_KHRONOS_validation"))
    {
        instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

        uint32_t layerExtCount = 0;
        vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerExtCount, nullptr);
        std::vector<VkExtensionProperties> layerExts(layerExtCount);
        vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &layerExtCount, layerExts.data());

        if (std::find_if(std::begin(layerExts), std::end(layerExts), [](const VkExtensionProperties& e) {
                return strcmp(e.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0;
            }) != std::end(layerExts))
        {
            instanceExts.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

            static const VkValidationFeatureEnableEXT validation_sync_features[1] = {
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
            };
            validationFeatures.enabledValidationFeatureCount = 1;
            validationFeatures.pEnabledValidationFeatures = validation_sync_features;
            instCreateInfo.pNext = &validationFeatures;
        }
    }
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////
    // inst create info
    instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instCreateInfo.pApplicationInfo = &appInfo;
    instCreateInfo.enabledLayerCount = (uint32_t)instanceLayers.size();
    instCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instCreateInfo.enabledExtensionCount = (uint32_t)instanceExts.size();
    instCreateInfo.ppEnabledExtensionNames = instanceExts.data();

    VkResult ret = vkCreateInstance(&instCreateInfo, nullptr, &instance);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Failed to create vkInstance.");
        return false;
    }

    // volk load instance
    volkLoadInstance(instance);

#ifdef VULKAN_DEBUG
    if (debugLayer)
    {
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        debugUtilsCreateInfo.messageSeverity = 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugUtilsCreateInfo.messageType = 
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = DebugCallback;
        debugUtilsCreateInfo.pUserData = this;
     
        res = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &mDebugUtilsMessenger);
        assert(res == VK_SUCCESS);
    }
#endif

    return true;
}

bool VulkanContext::CreateDevice(VkPhysicalDevice physicalDevice_, std::vector<const char*> deviceExt, std::vector<const char*> deviceLayers)
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // Find physical device

    physicalDevice = physicalDevice_;

    if (physicalDevice == VK_NULL_HANDLE)
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            Logger::Error("Failed to find gpus with vulkan support.");
            return false;
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            Logger::Info("Found vulkan gpu: %s", props.deviceName);
        }

        // Find best physical device
        U32 maxScore = 0;
        for (U32 i = (U32)devices.size(); i > 0; i--)
        {
            U32 score = 0;
            VkPhysicalDeviceProperties props = {};
            vkGetPhysicalDeviceProperties(devices[i - 1], &props);
            switch (props.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                score = 3;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                score = 2;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                score = 1;
            default:
                score = 0;
            }
            if (score >= maxScore)
            {
                physicalDevice = devices[i - 1];
                maxScore = score;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        Logger::Error("Failed to find a suitable GPU");
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Check extensions and layers
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> queriedExtensions(extCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, queriedExtensions.data());

    uint32_t layerCount = 0;
    vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, nullptr);
    std::vector<VkLayerProperties> queriedLayers(layerCount);
    if (layerCount > 0)
        vkEnumerateDeviceLayerProperties(physicalDevice, &layerCount, queriedLayers.data());

    const auto HasExtension = [&](const char* name)
    {
        auto it = std::find_if(std::begin(queriedExtensions), std::end(queriedExtensions), [name](const VkExtensionProperties& e) -> bool {
            return strcmp(e.extensionName, name) == 0;
            });
        return it != queriedExtensions.end();
    };

    const auto HasLayer = [&](const char* name)
    {
        auto it = std::find_if(std::begin(queriedLayers), std::end(queriedLayers), [name](const VkLayerProperties& e) -> bool {
            return strcmp(e.layerName, name) == 0;
            });
        return it != queriedLayers.end();
    };

    for (const auto& ext : deviceExt)
    {
        if (!HasExtension(ext))
            return false;
    }
    for (const auto& layer : deviceLayers)
    {
        if (!HasLayer(layer))
            return false;
    }

    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDevcieProps);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProps);

    Logger::Info("Selected vulkan gpu:%s", physicalDevcieProps.deviceName);
    if (physicalDevcieProps.apiVersion >= VK_API_VERSION_1_2)
    {
        ext.supportsVulkan12Device = ext.supportsVulkan12Instance;
    }

    std::vector<const char*> enabledExtensions;
    std::vector<const char*> enabledLayers;
    for (const auto& ext : deviceExt)
        enabledExtensions.push_back(ext);
    for (const auto& layer : deviceLayers)
        enabledLayers.push_back(layer);

#ifdef VULKAN_DEBUG
    if (debugLayer && HasLayer("VK_LAYER_KHRONOS_validation"))
        enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
    else if (debugLayer && HasLayer("VK_LAYER_LUNARG_standard_validation"))
        enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////
    // find queue families(Graphics, CopyFamily, Compute)

    // get queue framily props
    uint32_t queueFamilyCount = 0;
    if (ext.supportsVulkan12Instance && ext.supportsVulkan12Device)
        vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, nullptr);
    else if (ext.supportsPhysicalDeviceProperties2)
        vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, &queueFamilyCount, nullptr);
    else
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    queueFamilyProps.resize(queueFamilyCount);
    queueFamilyProps2.resize(queueFamilyCount);

    for (U32 i = 0; i < queueFamilyCount; i++)
        queueFamilyProps2[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;

    if (ext.supportsVulkan12Instance && ext.supportsVulkan12Device)
        vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyCount, queueFamilyProps2.data());
    else if (ext.supportsPhysicalDeviceProperties2)
        vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, &queueFamilyCount, queueFamilyProps2.data());
    else
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProps.data());

    if ((ext.supportsVulkan12Instance && ext.supportsVulkan12Device) || ext.supportsPhysicalDeviceProperties2)
        for (U32 i = 0; i < queueFamilyCount; i++)
            queueFamilyProps[i] = queueFamilyProps2[i].queueFamilyProperties;

    // find queue family indices
    for (int i = 0; i < (int)QUEUE_INDEX_COUNT; i++)
        queueInfo.familyIndices[i] = VK_QUEUE_FAMILY_IGNORED;

    std::vector<U32> queueOffset(queueFamilyCount);
    std::vector<std::array<F32, 4>> queuePriorities(queueFamilyCount);

    for (U32 famlilyIndex = 0; famlilyIndex < queueFamilyCount; famlilyIndex++)
        queueOffset[famlilyIndex] = 0;

    auto& queueIndices = queueInfo.familyIndices;
    const auto FindQueue = [&](QueueIndices index, VkQueueFlags target, VkQueueFlags ignored, float priority) ->bool
    {
        for (U32 famlilyIndex = 0; famlilyIndex < queueFamilyCount; famlilyIndex++)
        {
            auto& familyProp = queueFamilyProps[famlilyIndex];
            if (familyProp.queueFlags & ignored)
                continue;

            if (familyProp.queueCount > 0 && familyProp.queueFlags & target)
            {
                queueInfo.familyIndices[index] = famlilyIndex;
                familyProp.queueCount--;
                queueOffset[famlilyIndex]++;
                queuePriorities[famlilyIndex][index] = 1.0f;
                return true;
            }
        }

        return false;
    };

    // graphics queue
    if (!FindQueue(QUEUE_INDEX_GRAPHICS, VK_QUEUE_GRAPHICS_BIT, 0, 1.0f))
    {
        Logger::Error("Failed to find graphics queue.");
        return false;
    }

    // compute queue
    if (!FindQueue(QUEUE_INDEX_COMPUTE, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 1.0f) &&
        !FindQueue(QUEUE_INDEX_COMPUTE, VK_QUEUE_COMPUTE_BIT, 0, 1.0f))
    {
        Logger::Error("Failed to find compute queue.");
        return false;
    }

    // transfer queue
    if (!FindQueue(QUEUE_INDEX_TRANSFER, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 1.0f))
    {
        Logger::Error("Failed to find transfer queue.");
        return false;
    }

    // setup queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = {
        queueIndices[QUEUE_INDEX_GRAPHICS],
        queueIndices[QUEUE_INDEX_COMPUTE],
        queueIndices[QUEUE_INDEX_TRANSFER]
    };
    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies)
    {
        if (queueOffset[queueFamily] == 0)
            continue;

        VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = queueOffset[queueFamily];
        queueCreateInfo.pQueuePriorities = queuePriorities[queueFamily].data();
        queueCreateInfos.push_back(queueCreateInfo);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // setup device properties/features
    ext.properties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    ext.properties_1_1 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES };
    ext.properties_1_2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
    ext.properties2.pNext = &ext.properties_1_1;
    ext.properties_1_1.pNext = &ext.properties_1_2;
    void** properties_chain = &ext.properties_1_2.pNext;

    ext.features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
    ext.features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    ext.features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    ext.features2.pNext = &ext.features_1_1;
    ext.features_1_1.pNext = &ext.features_1_2;
    void** features_chain = &ext.features_1_2.pNext;

    bool checkFeatureChain = ext.supportsPhysicalDeviceProperties2 || (ext.supportsVulkan12Device && ext.supportsVulkan12Instance);
    if (checkFeatureChain)
    {
        if (HasExtension(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME))
        {
            //enabledExtensions.push_back(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);
            //ext.supportsDepthClip = true;
            //*features_chain = &ext.depthClipEnableFeatures;
            //features_chain = &ext.depthClipEnableFeatures.pNext;
        }
    }

    // Get properties
    if (ext.supportsVulkan12Instance && ext.supportsVulkan12Device)
        vkGetPhysicalDeviceProperties2(physicalDevice, &ext.properties2);
    else if (ext.supportsPhysicalDeviceProperties2)
        vkGetPhysicalDeviceProperties2KHR(physicalDevice, &ext.properties2);
    else
        vkGetPhysicalDeviceProperties(physicalDevice, &ext.properties2.properties);

    // Get features
    if (ext.supportsVulkan12Instance && ext.supportsVulkan12Device)
        vkGetPhysicalDeviceFeatures2(physicalDevice, &ext.features2);
    else if (ext.supportsPhysicalDeviceProperties2)
        vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &ext.features2);
    else
        vkGetPhysicalDeviceFeatures(physicalDevice, &ext.features2.features);
 
    ///////////////////////////////////////////////////////////////////////////////////////////
    // create device
    VkDeviceCreateInfo createDeviceInfo = {};
    createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createDeviceInfo.queueCreateInfoCount = (U32)queueCreateInfos.size();
    createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    createDeviceInfo.pEnabledFeatures = nullptr;
    createDeviceInfo.enabledExtensionCount = (U32)enabledExtensions.size();
    createDeviceInfo.ppEnabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();
    createDeviceInfo.enabledLayerCount = (U32)enabledLayers.size();
    createDeviceInfo.ppEnabledLayerNames = enabledLayers.empty() ? nullptr : enabledLayers.data();
    
    if (ext.supportsPhysicalDeviceProperties2)
        createDeviceInfo.pNext = &ext.features2;
    else
        createDeviceInfo.pEnabledFeatures = &ext.features2.features;

    if (vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device) != VK_SUCCESS)
        return false;

    volkLoadDevice(device);

    for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
    {
        if (queueIndices[i] != VK_QUEUE_FAMILY_IGNORED)
            vkGetDeviceQueue(device, queueIndices[i], 0, &queueInfo.queues[i]);
    }

    return true;
}

VkApplicationInfo VulkanContext::GetApplicationInfo()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cjing3D App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cjing3D Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    return appInfo;
}

#ifdef VULKAN_DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, 
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
    void* pUserData)
{
    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        // Using LINEAR filter with COMPARE. Spec bug, should not trigger validation.
        if (uint32_t(pCallbackData->messageIdNumber) == 0xf2fea78eu)
            return VK_FALSE;
        if (uint32_t(pCallbackData->messageIdNumber) == 0xd2c86c0cu)
            return VK_FALSE;
        if (uint32_t(pCallbackData->messageIdNumber) == 0x2aba6354u)
            return VK_FALSE;
    }

    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            Logger::Error("[Vulkan]: Validation Error: %s\n", pCallbackData->pMessage);
        else
            Logger::Error("[Vulkan]: Other Error: %s\n", pCallbackData->pMessage);
        break;

    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
            Logger::Warning("[Vulkan]: Validation Warning: %s\n", pCallbackData->pMessage);
        else
            Logger::Warning("[Vulkan]: Other Warning: %s\n", pCallbackData->pMessage);
        break;

    default:
        return VK_FALSE;
    }

    bool logObjectNames = false;
    for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
    {
        auto* name = pCallbackData->pObjects[i].pObjectName;
        if (name)
        {
            logObjectNames = true;
            break;
        }
    }

    if (logObjectNames)
    {
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
        {
            auto* name = pCallbackData->pObjects[i].pObjectName;
            Logger::Info("  Object #%u: %s\n", i, name ? name : "N/A");
        }
    }

    return VK_FALSE;
}
#endif

}
}