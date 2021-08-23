
#include "gpu.h"
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
    HMODULE dxcompiler = CiLoadLibrary("dxcompiler.dll");
    if (dxcompiler != nullptr)
    {
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)CiGetProcAddress(dxcompiler, "DxcCreateInstance");
        if (DxcCreateInstance != nullptr)
        {
            HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
            assert(SUCCEEDED(hr));
        }
    }

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
    Renderer::LoadShader(ShaderStage::VS, screenVS, "screenVS.hlsl");
    Renderer::LoadShader(ShaderStage::PS, screenPS, "screenPS.hlsl");

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