#pragma once

#include "definition.h"

#include <iostream>
#include <assert.h>
#include <vector>
#include <set>
#include <math.h>

#include "dxcompiler\inc\d3d12shader.h"
#include "dxcompiler\inc\dxcapi.h"

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

private:
    VkCommandPool mPool = VK_NULL_HANDLE;
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
    std::vector<VkFramebuffer> mFrameBuffers;
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
            vkDestroyFramebuffer(mDevice, mFrameBuffers[i], nullptr);
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

    struct FrameResource
    {
        CommandPool cmdPools[QueueIndices::QUEUE_INDEX_COUNT];
    };
    std::vector<FrameResource> mFrameResources;

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer);
    ~DeviceVulkan();

    bool InitSwapchain(uint32_t width, uint32_t height);
    bool CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader);

private:
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
