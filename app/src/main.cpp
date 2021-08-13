
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

#include <iostream>
#include <assert.h>
#include <vector>
#include <set>

#ifdef DEBUG
bool debugLayer = true;
#else
bool debugLayer = false;
#endif

class DeviceVulkan
{
private:
    VkDevice mDevice;
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};

    VkPhysicalDeviceFeatures2 mFeatures2 = {};
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    int mGraphicsFamily = -1;
    int mComputeFamily = -1;
    int mCopyFamily = -1;
    VkQueue mgGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mCopyQueue = VK_NULL_HANDLE;

public:
    DeviceVulkan(bool debugLayer) : mIsDebugLayer(debugLayer)
    {
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
        // enum extension names
        std::vector<const char*> extensionNames;
        for (auto& instanceExtension : availableInstanceExtensions)
        {
            if (strcmp(instanceExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            {
                mIsDebugUtils = true;
                extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            else if (strcmp(instanceExtension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
            {
                extensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            }
        }
        extensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);


        ///////////////////////////////////////////////////////////////////////////////////////////
        // enum layout instances
        std::vector<const char*> layoutInstances;
        if (debugLayer)
        {
            std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availablelayerProperties);
            layoutInstances.insert(layoutInstances.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
        }


        ///////////////////////////////////////////////////////////////////////////////////////////
        // inst create info
        VkInstanceCreateInfo instCreateInfo = {};
        instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instCreateInfo.pApplicationInfo = &appInfo;
        instCreateInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
        instCreateInfo.ppEnabledExtensionNames = extensionNames.data();
        instCreateInfo.enabledLayerCount = (uint32_t)layoutInstances.size();
        instCreateInfo.ppEnabledLayerNames = layoutInstances.data();

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
        if (vkCreateInstance(&instCreateInfo, nullptr, &mVkInstanc) != VK_SUCCESS)
        {
            std::cout << "Failed to create vkInstance." << std::endl;
            return;
        }

        // volk load instance
        volkLoadInstanceOnly(mVkInstanc);

        // create debug utils messager ext
        if (debugLayer && mIsDebugUtils)
        {
            res = vkCreateDebugUtilsMessengerEXT(mVkInstanc, &debugUtilsCreateInfo, nullptr, &mDebugUtilsMessenger);
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
        vkEnumeratePhysicalDevices(mVkInstanc, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            std::cout << "Failed to find gpus with vulkan support." << std::endl;
            return;
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(mVkInstanc, &deviceCount, devices.data());

        bool isBreak = false;
        for (const auto& device : devices)
        {
            if (CheckPhysicalSuitable(device, isBreak))
            {
                mPhysicalDevice = device;
                if (isBreak)
                    break;
            }
        }

        if (mPhysicalDevice == VK_NULL_HANDLE)
        {
            std::cout << "Failed to find a suitable GPU" << std::endl;
            return;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        // find queue families(Graphics, CopyFamily, Compute)
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);

        mQueueFamilies.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, mQueueFamilies.data());
    
        int familyIndex = 0;
        for (const auto& queueFamily : mQueueFamilies)
        {
            if (mGraphicsFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                mGraphicsFamily = familyIndex;
            }

            if (mCopyFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                mCopyFamily = familyIndex;
            }

            if (mComputeFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                mComputeFamily = familyIndex;
            }

            familyIndex++;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        // create logical device
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilies = { mGraphicsFamily, mCopyFamily, mComputeFamily };
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

        VkDeviceCreateInfo createDeviceInfo = {};
        createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createDeviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        createDeviceInfo.pEnabledFeatures = nullptr;
        createDeviceInfo.pNext = &mFeatures2;
        createDeviceInfo.enabledExtensionCount = (uint32_t)enabledDeviceExtensions.size();
        createDeviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

        res = vkCreateDevice(mPhysicalDevice, &createDeviceInfo, nullptr, &mDevice);
        assert(res == VK_SUCCESS);

        volkLoadDevice(mDevice);

        vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mgGraphicsQueue);
        vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mComputeQueue);
        vkGetDeviceQueue(mDevice, mCopyFamily, 0, &mCopyQueue);
    }

    ~DeviceVulkan()
    {
        if (mDebugUtilsMessenger != VK_NULL_HANDLE) {
            vkDestroyDebugUtilsMessengerEXT(mVkInstanc, mDebugUtilsMessenger, nullptr);
        }

        vkDestroyDevice(mDevice, nullptr);
        vkDestroyInstance(mVkInstanc, nullptr);
    }

private:
    std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& layerProperties)
    {
        std::vector<const char*> ret;
        ret.push_back("VK_LAYER_KHRONOS_validation");
        return ret;
    }

    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak)
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
        bool ret = mPhysicalDevice == VK_NULL_HANDLE;
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

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions)
    {
        for (const auto& x : availableExtensions)
        {
            if (strcmp(x.extensionName, checkExtension) == 0)
                return true;
        }
        return false;
    }
};


int main()
{
    // init glfw
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Test", nullptr, nullptr);

    // init gpu
    DeviceVulkan* deviceVulkan = new DeviceVulkan(debugLayer);
    
    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    // uninit
    delete deviceVulkan;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}