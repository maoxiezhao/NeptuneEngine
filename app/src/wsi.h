#pragma once

#include "common.h"
#include "vulkan\definition.h"

class Platform;
struct Swapchain;

class WSI
{
public:
	bool Initialize();
	void Uninitialize();
	void BeginFrame();
	void EndFrame();
	void SetPlatform(Platform* platform);
	Swapchain* GetSwapChain();
	
	DeviceVulkan* GetDevice();

private:
	Platform* mPlatform = nullptr;
};