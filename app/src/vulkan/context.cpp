#include "context.h"

#include <set>

namespace GPU
{

namespace
{
    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions)
    {
        for (const auto& x : availableExtensions)
        {
            if (strcmp(x.extensionName, checkExtension) == 0)
                return true;
        }
        return false;
    }

    std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& layerProperties)
    {
        std::vector<std::vector<const char*>> validationLayerPriorityList =
        {
            // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
            {"VK_LAYER_KHRONOS_validation"},

            // Otherwise we fallback to using the LunarG meta layer
            {"VK_LAYER_LUNARG_standard_validation"},

            // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
            {
                "VK_LAYER_GOOGLE_threading",
                "VK_LAYER_LUNARG_parameter_validation",
                "VK_LAYER_LUNARG_object_tracker",
                "VK_LAYER_LUNARG_core_validation",
                "VK_LAYER_GOOGLE_unique_objects",
            },

            // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
            {"VK_LAYER_LUNARG_core_validation"}
        };

        auto ValidateLayers = [](const std::vector<const char*>& required,
            const std::vector<VkLayerProperties>& available)
        {
            for (auto layer : required)
            {
                bool found = false;
                for (auto& available_layer : available)
                {
                    if (strcmp(available_layer.layerName, layer) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }

            return true;
        };

        for (auto& validationLayers : validationLayerPriorityList)
        {
            if (ValidateLayers(validationLayers, layerProperties))
            {
                return validationLayers;
            }
        }

        return {};
    }


    
}

VulkanContext::~VulkanContext()
{
    if (mDebugUtilsMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance, mDebugUtilsMessenger, nullptr);
    }

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

bool VulkanContext::Initialize(std::vector<const char*> instanceExt, bool debugLayer)
{
    mIsDebugLayer = debugLayer;

    VkResult res = volkInitialize();
    assert(res == VK_SUCCESS);

    // app info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cjing3D App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cjing3D Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // enum available layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availablelayerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availablelayerProperties.data());

    // enum available extension
    uint32_t extensionCount = 0;
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data());
    assert(res == VK_SUCCESS);


    ///////////////////////////////////////////////////////////////////////////////////////////
    // check features by instance extensions
    auto itr = std::find_if(instanceExt.begin(), instanceExt.end(), [](const char* name) {
        return strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        });
    bool has_surface_extension = itr != instanceExt.end();
    if (has_surface_extension && CheckExtensionSupport(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, availableInstanceExtensions))
    {
        instanceExt.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        mExtensionFeatures.SupportsSurfaceCapabilities2 = true;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // enum layout instances
    std::vector<const char*> instanceLayers;
    if (debugLayer)
    {
        std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availablelayerProperties);
        instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // inst create info
    VkInstanceCreateInfo instCreateInfo = {};
    instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instCreateInfo.pApplicationInfo = &appInfo;
    instCreateInfo.enabledLayerCount = (uint32_t)instanceLayers.size();
    instCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instCreateInfo.enabledExtensionCount = (uint32_t)instanceExt.size();
    instCreateInfo.ppEnabledExtensionNames = instanceExt.data();

    // debug info
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    if (debugLayer && mIsDebugUtils)
    {
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = DebugCallback;
        instCreateInfo.pNext = &debugUtilsCreateInfo;
    }

    // create instance
    VkResult ret = vkCreateInstance(&instCreateInfo, nullptr, &instance);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Failed to create vkInstance.");
        return false;
    }

    // volk load instance
    volkLoadInstanceOnly(instance);

    // create debug utils messager ext
    if (debugLayer && mIsDebugUtils)
    {
        res = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &mDebugUtilsMessenger);
        assert(res == VK_SUCCESS);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // enum physical devices
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,                // swapchain
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,        // depth clip
    };
    std::vector<const char*> enabledDeviceExtensions;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        Logger::Error("Failed to find gpus with vulkan support.");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    bool isBreak = false;
    for (const auto& device : devices)
    {
        if (CheckPhysicalSuitable(device, isBreak))
        {
            enabledDeviceExtensions = requiredDeviceExtensions;

            physicalDevice = device;
            if (isBreak)
                break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        Logger::Error("Failed to find a suitable GPU");
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // find queue families(Graphics, CopyFamily, Compute)
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    mQueueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, mQueueFamilies.data());

    for (int i = 0; i < (int)QUEUE_INDEX_COUNT; i++)
        queueInfo.mQueueIndices[i] = VK_QUEUE_FAMILY_IGNORED;

    auto& queueIndices = queueInfo.mQueueIndices;
    int familyIndex = 0;
    for (const auto& queueFamily : mQueueFamilies)
    {
        if (queueIndices[QUEUE_INDEX_GRAPHICS] == VK_QUEUE_FAMILY_IGNORED && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueIndices[QUEUE_INDEX_GRAPHICS] = familyIndex;

        if (queueIndices[QUEUE_INDEX_TRANSFER] == VK_QUEUE_FAMILY_IGNORED && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            queueIndices[QUEUE_INDEX_TRANSFER] = familyIndex;

        if (queueIndices[QUEUE_INDEX_COMPUTE] == VK_QUEUE_FAMILY_IGNORED && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            queueIndices[QUEUE_INDEX_COMPUTE] = familyIndex;

        familyIndex++;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // create logical device

    // setup queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = {
        queueIndices[QUEUE_INDEX_GRAPHICS],
        queueIndices[QUEUE_INDEX_COMPUTE],
        queueIndices[QUEUE_INDEX_GRAPHICS]
    };
    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // create device
    VkDeviceCreateInfo createDeviceInfo = {};
    createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createDeviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    createDeviceInfo.pEnabledFeatures = nullptr;
    createDeviceInfo.pNext = &mFeatures2;
    createDeviceInfo.enabledExtensionCount = (uint32_t)enabledDeviceExtensions.size();
    createDeviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

    res = vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device);
    assert(res == VK_SUCCESS);

    volkLoadDevice(device);

    for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
        vkGetDeviceQueue(device, queueIndices[i], 0, &queueInfo.mQueues[i]);

    return true;
}

bool VulkanContext::CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak)
{
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
    };

    // 1. check device support required device extensions
    uint32_t extensionCount;
    VkResult res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
    res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.data());
    assert(res == VK_SUCCESS);

    for (auto& requiredExt : requiredDeviceExtensions)
    {
        if (!CheckExtensionSupport(requiredExt, availableDeviceExtensions))
            return false;
    }

    // 2. check device properties
    bool ret = physicalDevice == VK_NULL_HANDLE;
    vkGetPhysicalDeviceProperties2(device, &mProperties2);
    if (mProperties2.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        isBreak = true;
        ret = true;
    }

    // 3. check device features
    mFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(device, &mFeatures2);

    ret &= mFeatures2.features.imageCubeArray == VK_TRUE;
    ret &= mFeatures2.features.independentBlend == VK_TRUE;
    ret &= mFeatures2.features.geometryShader == VK_TRUE;
    ret &= mFeatures2.features.samplerAnisotropy == VK_TRUE;
    ret &= mFeatures2.features.shaderClipDistance == VK_TRUE;
    ret &= mFeatures2.features.textureCompressionBC == VK_TRUE;
    ret &= mFeatures2.features.occlusionQueryPrecise == VK_TRUE;

    return ret;
}

}