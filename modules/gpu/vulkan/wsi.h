#pragma once

#include "core\common.h"
#include "core\platform\platform.h"
#include "device.h"

namespace VulkanTest
{
class WSI;

enum class PresentMode
{
	SyncToVBlank,		  // Force FIFO
	UnlockedMaybeTear,    // MAILBOX or IMMEDIATE
	UnlockedForceTearing, // Force IMMEDIATE
	UnlockedNoTearing     // Force MAILBOX
};

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

	virtual bool Init(int width_, int height_, const char* title) = 0;
	virtual U32  GetWidth() = 0;
	virtual U32  GetHeight() = 0;
	virtual void BlockWSI(WSI& wsi) = 0;
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

	bool Initialize(U32 numThread);
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(WSIPlatform* platform_);
	WSIPlatform* GetPlatform();

	GPU::DeviceVulkan* GetDevice();
	VkFormat GetSwapchainFormat()const;

private:
	bool InitSwapchain(U32 width, U32 height);
	enum class SwapchainError
	{
		None,
		NoSurface,
		Error
	};
	SwapchainError InitSwapchainImpl(U32 width, U32 height);

	void UpdateFrameBuffer(U32 width, U32 height);
	void DrainSwapchain();
	void TeardownSwapchain();
	
	WSIPlatform* platform = nullptr;
	PresentMode presentMode = PresentMode::SyncToVBlank;
};
}