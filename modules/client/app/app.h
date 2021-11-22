#pragma once

#include "core\common.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{
class App
{
public:
	App() = default;
	~App() = default;

	bool Initialize(std::unique_ptr<WSIPlatform> platform_);
	void Uninitialize();
	bool Poll();
	void Tick();

private:
	virtual void Setup();
	virtual void InitializeImpl();
	virtual void Render();
	virtual void UninitializeImpl();

protected:
	std::unique_ptr<WSIPlatform> platform;
	WSI wsi;
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[]);
}