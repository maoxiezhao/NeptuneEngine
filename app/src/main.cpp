
#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

#include <iostream>
#include <assert.h>

int main()
{
    // init glfw
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Test", nullptr, nullptr);

    // init gpu
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

    // inst create info
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = glfwExtensionCount;
    instInfo.ppEnabledExtensionNames = glfwExtensions;
    instInfo.enabledLayerCount = 0;

    VkInstance vkInstance;
    if (vkCreateInstance(&instInfo, nullptr, &vkInstance) != VK_SUCCESS)
    {
        std::cout << "Failed to create vkInstance." << std::endl;
        return 0;
    }

    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    // uninit
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}