#pragma once

#include "core\common.h"
#include "device.h"

namespace VulkanTest
{
class Platform;

enum class PresentMode
{
	SyncToVBlank,		  // Force FIFO
	UnlockedMaybeTear,    // MAILBOX or IMMEDIATE
	UnlockedForceTearing, // Force IMMEDIATE
	UnlockedNoTearing     // Force MAILBOX
};

class WSIPlatform
{
public:
	virtual ~WSIPlatform() = default;

	virtual std::vector<const char*> GetRequiredExtensions(bool debugUtils) = 0;
	virtual std::vector<const char*> GetRequiredDeviceExtensions()
	{
		return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}
	virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;

	virtual void PollInput() = 0;
	virtual U32 GetWidth() = 0;
	virtual U32 GetHeight() = 0;
	virtual bool IsAlived() = 0;

	bool ShouldResize()const 
	{
		return isResize; 
	}

	virtual void NotifySwapchainDimensions(unsigned width, unsigned height)
	{
		swapchainWidth = width;
		swapchainHeight = height;
		isResize = false;
	}

protected:
	U32 swapchainWidth = 0;
	U32 swapchainHeight = 0;
	bool isResize = false;
};

class WSI
{
public:
	WSI();
	~WSI();

	bool Initialize(U32 numThread);
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(WSIPlatform* platform_);

	GPU::DeviceVulkan* GetDevice();

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