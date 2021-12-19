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

	bool InitWSI(std::unique_ptr<WSIPlatform> platform_);
	void Initialize();
	void Uninitialize();
	bool Poll();
	void RunFrame();

	WSI& GetWSI()
	{
		return wsi;
	}

	WSIPlatform& GetPlatform()
	{
		return *platform;
	}

private:
	virtual void InitializeImpl() {};
	virtual void UninitializeImpl() {};
	virtual void Render() {};

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