#pragma once

// platform win32
#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>

#define VK_USE_PLATFORM_WIN32_KHR
#endif

// vulkan
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

#include <iostream>
#include <assert.h>
#include <vector>
#include <set>
#include <math.h>

#include "dxcompiler\inc\d3d12shader.h"
#include "dxcompiler\inc\dxcapi.h"

class DeviceVulkan;

enum class ShaderStage
{
    MS = 0,
    AS,
    VS,
    HS,
    DS,
    GS,
    PS,
    CS,
    LIB,
    Count
};

struct PipelineLayout
{
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
};

struct Shader
{
    ShaderStage mShaderStage;
    VkShaderModule mShaderModule = VK_NULL_HANDLE;
};

struct ShaderProgram
{
public:
    PipelineLayout* GetPipelineLayout()const
    {
        return mPipelineLayout;
    }

    inline const Shader* GetShader(ShaderStage stage) const
    {
        return mShaders[UINT(stage)];
    }

private:
    void SetShader(ShaderStage stage, Shader* handle);

    DeviceVulkan* mDevice;
    PipelineLayout* mPipelineLayout = nullptr;;
    Shader* mShaders[UINT(ShaderStage::Count)] = {};
};

struct RenderPass
{
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
};

struct PipelineStateDesc
{
    ShaderProgram mShaderProgram;
};

struct CommandList
{
public:
    VkDevice& mDevice;
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;

    RenderPass* mRenderPass = nullptr;
    bool mIsDirtyPipeline = true;

    PipelineStateDesc mPipelineStateDesc = {};

public:
    CommandList(VkDevice& device);
    ~CommandList();

    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    VkPipeline BuildGraphicsPipeline(const PipelineStateDesc& pipelineStateDesc);
};

struct DeviceFeatures
{
    bool SupportsSurfaceCapabilities2 = false;
};

struct Swapchain
{
    VkDevice& mDevice;
    VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR mFormat;
    VkExtent2D mSwapchainExtent;
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;
    RenderPass mDefaultRenderPass;
    VkSemaphore mSwapchainAcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore mSwapchainReleaseSemaphore = VK_NULL_HANDLE;

    Swapchain(VkDevice& device) : mDevice(device)
    {
    }

    ~Swapchain()
    {
        for (int i = 0; i < mImageViews.size(); i++)
        {
            vkDestroyImageView(mDevice, mImageViews[i], nullptr);
        }

        vkDestroySemaphore(mDevice, mSwapchainAcquireSemaphore, nullptr);
        vkDestroySemaphore(mDevice, mSwapchainReleaseSemaphore, nullptr);
        vkDestroyRenderPass(mDevice, mDefaultRenderPass.mRenderPass, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    }
};

class DeviceVulkan
{
public:
    GLFWwindow* mWindow;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    VkDevice mDevice;
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};

    DeviceFeatures mExtensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    int mGraphicsFamily = -1;
    int mComputeFamily = -1;
    int mCopyFamily = -1;
    VkQueue mgGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mCopyQueue = VK_NULL_HANDLE;

    VkSurfaceKHR mSurface;
    Swapchain* mSwapchain = nullptr;

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer);
    ~DeviceVulkan();

    bool CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader);

private:
    bool InitSwapchain(VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height);
    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak);
    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions);
    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice);
    std::vector<const char*> GetRequiredExtensions();

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
