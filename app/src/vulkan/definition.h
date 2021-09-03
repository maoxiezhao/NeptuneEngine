#pragma once

#include "common.h"

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

enum QueueIndices
{
    QUEUE_INDEX_GRAPHICS,
    QUEUE_INDEX_COMPUTE,
    QUEUE_INDEX_TRANSFER,
    QUEUE_INDEX_VIDEO_DECODE,
    QUEUE_INDEX_COUNT
};

enum class QueueType
{
    GRAPHICS = QUEUE_INDEX_GRAPHICS,
    Count
};


class DeviceVulkan;

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

struct RenderPassInfo
{

};

struct FrameBuffer
{
    VkFramebuffer mFrameBuffer = VK_NULL_HANDLE;
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
