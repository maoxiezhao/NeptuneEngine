#pragma once

#include "core\common.h"
#include "core\engine.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{

class App
{
public:
    const U32 DEFAULT_WIDTH = 1280;
    const U32 DEFAULT_HEIGHT = 720;

	App();
	virtual ~App();

	bool InitializeWSI(std::unique_ptr<WSIPlatform> platform_);
	bool Poll();
	void RunFrame();

	virtual void Initialize();
	virtual void Uninitialize();
	virtual void Render();

	WSI& GetWSI()
	{
		return wsi;
	}

	WSIPlatform& GetPlatform()
	{
		return *platform;
	}

	virtual U32 GetDefaultWidth()
	{
		return DEFAULT_WIDTH;
	}

	virtual U32 GetDefaultHeight()
	{
		return DEFAULT_HEIGHT;
	}

	virtual const char* GetWindowTitle()
	{
		return "VULKAN_TEST";
	}

protected:
	
	void request_shutdown()
	{
		requestedShutdown = true;
	}

protected:
	std::unique_ptr<WSIPlatform> platform;
	WSI wsi;
	bool requestedShutdown = false;
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[]);
}