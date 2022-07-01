#pragma once

#include "core\common.h"
#include "core\engine.h"
#include "core\platform\timer.h"
#include "core\platform\platform.h"
#include "core\scene\world.h"
#include "gpu\vulkan\wsi.h"
#include "renderer\renderPath.h"

namespace VulkanTest
{

class VULKAN_TEST_API App
{
public:
    const U32 DEFAULT_WIDTH = 1280;
    const U32 DEFAULT_HEIGHT = 720;

	App();
	virtual ~App();

	void Run(std::unique_ptr<WSIPlatform> platform_);

	WSI& GetWSI() {
		return wsi;
	}

	WSIPlatform& GetPlatform() {
		return *platform;
	}

	Engine& GetEngine() {
		return *engine;
	}

	struct RendererPlugin* GetRenderer() {
		return renderer;
	}

	virtual U32 GetDefaultWidth() {
		return DEFAULT_WIDTH;
	}

	virtual U32 GetDefaultHeight() {
		return DEFAULT_HEIGHT;
	}

	virtual const char* GetWindowTitle() {
		return "VULKAN_TEST";
	}

	F32 GetDeltaTime()const {
		return deltaTime;
	}

	F32 GetLastDeltaTime()const {
		return smoothTimeDelta;
	}

protected:	
	bool Poll();
	void OnIdle();

	virtual void Initialize();
	virtual void Uninitialize();
	virtual void OnEvent(const Platform::WindowEvent& ent);

	void RequestShutdown()
	{
		requestedShutdown = true;
	}

	virtual void Update(F32 deltaTime);
	virtual void FixedUpdate();
	virtual void Render();

	void ComputeSmoothTimeDelta();

	void ActivePath(RenderPath* renderPath_);
	inline RenderPath* GetActivePath() { return renderPath; }

protected:
	std::unique_ptr<WSIPlatform> platform;
	UniquePtr<Engine> engine;
	WSI wsi;
	Timer timer;
	F32 deltaTime = 0.0f;
	F32 targetFrameRate = 60;
	F32 deltaTimeAcc = 0;
	F32 lastTimeDeltas[11] = {};
	U32 lastTimeFrames = 0;
	F32 smoothTimeDelta = 0.0f;

	bool frameskip = true;
	bool framerateLock = false;
	bool requestedShutdown = false;
	World* world = nullptr;
	struct RendererPlugin* renderer = nullptr;
	RenderPath* renderPath = nullptr;
};

UniquePtr<Engine> CreateEngine(const Engine::InitConfig& config, App& app);
int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[]);
}