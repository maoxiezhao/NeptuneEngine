#pragma once

#include "core\common.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{

class App
{
public:
	App();
	virtual ~App();

	void SetPlatform(std::unique_ptr<WSIPlatform> platform_);
	bool InitializeWSI();
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
		return 1280;
	}

	virtual U32 GetDefaultHeight()
	{
		return 720;
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