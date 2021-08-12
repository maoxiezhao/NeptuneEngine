
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

#include <iostream>
#include <assert.h>
#include <vector>

#ifdef DEBUG
bool debugLayer = true;
#else
bool debugLayer = false;
#endif

class DeviceVulkan
{
private:
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

public:
    DeviceVulkan(bool debugLayer) : mIsDebugLayer(debugLayer)
    {
        VkResult res = volkInitialize();
        assert(res == VK_SUCCESS);

        // app info
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

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

        // enum layout instances
        std::vector<const char*> layoutInstances;
        if (debugLayer)
        {
            std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availablelayerProperties);
            layoutInstances.insert(layoutInstances.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
        }

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
            res = vkCreateDebugUtilsMessengerEXT(mVkInstanc, &debugUtilsCreateInfo, nullptr, &debugUtilsMessenger);
            assert(res == VK_SUCCESS);
        }
    }

    ~DeviceVulkan()
    {
        if (debugUtilsMessenger != VK_NULL_HANDLE) {
            vkDestroyDebugUtilsMessengerEXT(mVkInstanc, debugUtilsMessenger, nullptr);
        }

        vkDestroyInstance(mVkInstanc, nullptr);
    }

private:
    std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& layerProperties)
    {
        std::vector<const char*> ret;
        ret.push_back("VK_LAYER_KHRONOS_validation");
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