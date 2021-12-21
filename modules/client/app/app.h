#pragma once

#include "core\common.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{

class App
{
public:
	struct InitConfig
	{
		const char* workingDir = nullptr;
		const char* windowTitle = "VULKAN_TEST";
		Span<const char*> plugins;
	};

	App(const InitConfig& initConfig_);
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

	InitConfig GetInitConfig()const
	{
		return initConfig;
	}

	virtual U32 GetDefaultWidth()
	{
		return 1280;
	}

	virtual U32 GetDefaultHeight()
	{
		return 720;
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
	InitConfig initConfig;
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[]);
}