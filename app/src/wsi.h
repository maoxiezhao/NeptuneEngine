#pragma once

#include "common.h"
#include "vulkan\device.h"

class Platform;

class WSI
{
public:
	bool Initialize();
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(Platform* platform);

	GPU::Swapchain* GetSwapChain();
	GPU::DeviceVulkan* GetDevice();

private:
	Platform* platform = nullptr;
};