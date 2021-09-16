#pragma once

#include <memory>

#include "wsi.h"
#include "vulkan\definition.h"

class Platform
{
public:
	Platform();
	~Platform();

	bool Init(int width, int height);
	bool Poll();

	uint32_t GetWidth()
	{
		return mWidth;
	}

	uint32_t GetHeight()
	{
		return mHeight;
	}

	GLFWwindow* GetWindow() {
		return mWindow;
	}

private:
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
	GLFWwindow* mWindow = nullptr;
};

class App
{
public:
	App() = default;
	~App() = default;

	bool Initialize(std::unique_ptr<Platform> platform);
	void Uninitialize();
	bool Poll();
	void Tick();

private:
	virtual void InitializeImpl();
	virtual void Render();
	virtual void UninitializeImpl();

protected:
	std::unique_ptr<Platform> mPlatform;
	WSI mWSI;
};