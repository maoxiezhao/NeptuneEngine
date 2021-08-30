#pragma once

#include "definition.h"
#include "common.h"

#include <set>

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

    VkCommandBuffer RequestCommandBuffer();

private:
    uint32_t mUsedIndex = 0;
    DeviceVulkan* mDevice;
    VkCommandPool mPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mBuffers;
};

struct CommandList
{
public:
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;

    VkFramebuffer mFrameBuffer = VK_NULL_HANDLE;
    RenderPass* mRenderPass = nullptr;
    bool mIsDirtyPipeline = true;
    PipelineStateDesc mPipelineStateDesc = {};
    

    DeviceVulkan& mDevice;
    VkCommandBuffer mBuffer;
    QueueType mType;

public:
    CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();

private:
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
    // TODO: do it in wsi
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

    // per frame resource
    struct FrameResource
    {
        CommandPool cmdPools[QueueIndices::QUEUE_INDEX_COUNT];
    };
    std::vector<FrameResource> mFrameResources;
    uint32_t mFrameIndex = 0;

    FrameResource& CurrentFrameResource()
    {
        assert(mFrameIndex < mFrameResources.size());
        return mFrameResources[mFrameIndex];
    }

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer);
    ~DeviceVulkan();

    bool InitSwapchain(uint32_t width, uint32_t height);
    bool CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader);
    CommandList* RequestCommandList(QueueType queueType);
    VkFramebuffer RequestFrameBuffer(const RenderPassInfo& renderPassInfo);

private:
    // initial methods
    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak);
    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions);
    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice);
    std::vector<const char*> GetRequiredExtensions();

    // debug callback
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
