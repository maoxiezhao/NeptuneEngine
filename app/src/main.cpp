
#include "vulkan/gpu.h"
#include "shaderCompiler.h"

#ifdef DEBUG
static bool debugLayer = true;
#else
static bool debugLayer = false;
#endif

Shader screenVS;
Shader screenPS;

int main()
{
    // init dxcompiler
    ShaderCompiler::Initialize();

    ///////////////////////////////////////////////////////////////////////////////
    // init glfw
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Test", nullptr, nullptr);

    // init gpu
    DeviceVulkan* deviceVulkan = new DeviceVulkan(window, debugLayer);
    
    // init test shader
    ShaderCompiler::LoadShader(*deviceVulkan, ShaderStage::VS, screenVS, "screenVS.hlsl");
    ShaderCompiler::LoadShader(*deviceVulkan, ShaderStage::PS, screenPS, "screenPS.hlsl");

    //deviceVulkan->CreateShader(ShaderStage::VS, nullptr, 0, &testVertShader);

    //////////////////////////////////////////////////////////////////////////////
    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    //////////////////////////////////////////////////////////////////////////////
    // uninit

    if (screenVS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(deviceVulkan->mDevice, screenVS.mShaderModule, nullptr);

    delete deviceVulkan;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}