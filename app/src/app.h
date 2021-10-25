#pragma once

#include "core\common.h"
#include "wsi.h"
#include "GLFW\glfw3.h"

class Platform
{
public:
	Platform();
	~Platform();

	bool Init(int width_, int height_);
	bool Poll();

	uint32_t GetWidth()
	{
		return width;
	}

	uint32_t GetHeight()
	{
		return height;
	}

	GLFWwindow* GetWindow() {
		return window;
	}

	void SetSize(uint32_t w, uint32_t h)
	{
		width = w;
		height = h;
	}

private:
	uint32_t width = 0;
	uint32_t height = 0;
	GLFWwindow* window = nullptr;
};

class App
{
public:
	App() = default;
	~App() = default;

	bool Initialize(std::unique_ptr<Platform> platform_);
	void Uninitialize();
	bool Poll();
	void Tick();

private:
	virtual void Setup();
	virtual void InitializeImpl();
	virtual void Render();
	virtual void UninitializeImpl();

protected:
	std::unique_ptr<Platform> platform;
	WSI wsi;
};