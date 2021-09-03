#pragma once

#include "definition.h"
#include "common.h"
#include "utils\objectPool.h"
#include "utils\intrusivePtr.hpp"

#include <set>
#include <unordered_map>

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(CommandPool&&) noexcept;
    CommandPool& operator=(CommandPool&&) noexcept;

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

    VkCommandBuffer RequestCommandBuffer();
    void BeginFrame();

private:
    uint32_t mUsedIndex = 0;
    DeviceVulkan* mDevice;
    VkCommandPool mPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mBuffers;
};


class CommandList;
struct CommandListDeleter
{
    void operator()(CommandList* cmd);
};
class CommandList : public Util::IntrusivePtrEnabled<CommandList, CommandListDeleter>
{
public:
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;
    bool mIsDirtyPipeline = true;
    PipelineStateDesc mPipelineStateDesc = {};

    DeviceVulkan& mDevice;
    VkCommandBuffer mCmd;
    QueueType mType;

    // render pass runtime 
    FrameBuffer* mFrameBuffer = nullptr;
    RenderPass* mRenderPass = nullptr;

public:
    CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void EndCommandBuffer();

private:
    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    VkPipeline BuildGraphicsPipeline(const PipelineStateDesc& pipelineStateDesc);
};
using CommandListPtr = Util::IntrusivePtr<CommandList>;

struct DeviceFeatures
{
    bool SupportsSurfaceCapabilities2 = false;
};

class DeviceVulkan
{
public:
    // base info
    GLFWwindow* mWindow;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    // core 
    VkDevice mDevice;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface;
    
    // features
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};
    DeviceFeatures mExtensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};

    // queue
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    int mGraphicsFamily = -1;
    int mComputeFamily = -1;
    int mCopyFamily = -1;
    VkQueue mgGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mCopyQueue = VK_NULL_HANDLE;

    // per frame resource
    struct FrameResource
    {
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];
        void Begin();
    };
    std::vector<FrameResource> mFrameResources;
    uint32_t mFrameIndex = 0;

    FrameResource& CurrentFrameResource()
    {
        assert(mFrameIndex < mFrameResources.size());
        return mFrameResources[mFrameIndex];
    }

    // vulkan object pools
    std::unordered_map<uint64_t, RenderPass> mRenderPasses;
    std::unordered_map<uint64_t, FrameBuffer> mFrameBuffers;
    Util::ObjectPool<CommandList> mCommandListPool;

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer);
    ~DeviceVulkan();

    bool CreateSwapchain(Swapchain*& swapchain, uint32_t width, uint32_t height);
    bool CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader);
    
    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandList(int threadIndex, QueueType queueType);
    
    RenderPassInfo GetSwapchianRenderPassInfo();
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);

    void BeginFrameContext();
    void EndFrameContext();
    void Submit(CommandListPtr& cmd);

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
