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

	virtual bool Poll() = 0;
	virtual U32 GetWidth() = 0;
	virtual U32 GetHeight() = 0;
};

class WSI
{
public:
	bool Initialize();
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(WSIPlatform* platform_);

	GPU::DeviceVulkan* GetDevice();

private:
	bool InitSwapchain(uint32_t width, uint32_t height);
	void TeardownSwapchain();
	
	WSIPlatform* platform = nullptr;
	PresentMode presentMode = PresentMode::SyncToVBlank;
};
}