#pragma once

#include "core\common.h"
#include "core\engine.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\context.h"

namespace VulkanTest
{
class WSI;

class VULKAN_TEST_API WSIPlatform
{
public:
	virtual ~WSIPlatform() = default;

	virtual std::vector<const char*> GetRequiredExtensions(bool debugUtils) = 0;
	virtual std::vector<const char*> GetRequiredDeviceExtensions()
	{
		return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;
	virtual VkSurfaceKHR CreateSurface(VkInstance instance, Platform::WindowType window) = 0;

	virtual bool Init(int width_, int height_, const char* title) = 0;
	virtual U32  GetWidth() = 0;
	virtual U32  GetHeight() = 0;
	virtual Platform::WindowType GetWindow() = 0;
	virtual void NotifyResize(U32 width_, U32 height_) = 0;

	bool ShouldResize()const 
	{
		return resize; 
	}

	virtual void NotifySwapchainDimensions(unsigned width, unsigned height)
	{
		swapchainWidth = width;
		swapchainHeight = height;
		resize = false;
	}

	virtual void OnDeviceCreated(GPU::DeviceVulkan* device);
	virtual void OnDeviceDestroyed();
	virtual void OnSwapchainCreated(GPU::DeviceVulkan* device, U32 width, U32 height, float aspectRatio, size_t numSwapchainImages, VkFormat format, VkSurfaceTransformFlagBitsKHR preRotate);
	virtual void OnSwapchainDestroyed();
	
protected:
	U32 swapchainWidth = 0;
	U32 swapchainHeight = 0;
	bool resize = false;
};

class VULKAN_TEST_API WSI
{
public:
	WSI();
	~WSI();

	bool Initialize(GPU::SystemHandles& handles, U32 numThread);
	bool InitializeExternal(VkSurfaceKHR surface_, GPU::DeviceVulkan& device_, GPU::VulkanContext& context_, I32 width, I32 height);
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void PresentBegin();
	void PresentEnd();
	void UpdateFrameBuffer(U32 width, U32 height);
	void SetPlatform(WSIPlatform* platform_);
	WSIPlatform* GetPlatform();

	GPU::DeviceVulkan* GetDevice();
	GPU::VulkanContext* GetContext();
	VkFormat GetSwapchainFormat()const;
	GPU::ImageView& GetImageView() { return swapchain.images[swapchain.swapchainImageIndex]->GetImageView(); }
	VkSurfaceKHR GetSurface()const { return surface; }
	GPU::SwapChain& GetSwapchain() { return swapchain; }

private:
	bool InitSwapchain(U32 width, U32 height);
	void DrainSwapchain();
	void TeardownSwapchain();

	WSIPlatform* platform = nullptr;
	bool isExternal = false;

	GPU::VulkanContext* vulkanContext = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	GPU::DeviceVulkan* deviceVulkan = nullptr;
	GPU::SwapChain swapchain = {};
	bool vsync = true;
	bool isSwapchinSuboptimal = false;
	bool swapchainIndexHasAcquired = false;
	std::vector<GPU::SemaphorePtr> releaseSemaphores;
};
}