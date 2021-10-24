#pragma once

#include "common.h"
#include "vulkan\device.h"

class Platform;

enum class PresentMode
{
	SyncToVBlank,		  // Force FIFO
	UnlockedMaybeTear,    // MAILBOX or IMMEDIATE
	UnlockedForceTearing, // Force IMMEDIATE
	UnlockedNoTearing     // Force MAILBOX
};

class WSI
{
public:
	bool Initialize();
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(Platform* platform_);

	GPU::DeviceVulkan* GetDevice();

private:
	bool InitSwapchain(uint32_t width, uint32_t height);
	void TeardownSwapchain();
	
	Platform* platform = nullptr;
	PresentMode presentMode = PresentMode::SyncToVBlank;
};