#include "app\app.h"
#include "core\memory\memory.h"
#include "core\utils\profiler.h"
#include "gpu\vulkan\wsi.h"
#include "GLFW\glfw3.h"

#include <thread>

namespace VulkanTest
{
class PlatformGFLW : public WSIPlatform
{
private:
	uint32_t width = 0;
	uint32_t height = 0;
	GLFWwindow* window = nullptr;
	bool asyncLoopAlive = true;
	std::atomic_bool requestClose;
	std::thread mainLoop;

public:
	PlatformGFLW() = default;

	virtual ~PlatformGFLW()
	{
		if (window != nullptr)
		{
			glfwDestroyWindow(window);
			glfwTerminate();
		}
	}

	bool Init(int width_, int height_)
	{
		requestClose.store(false);

		width = width_;
		height = height_;

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		// create window
		window = glfwCreateWindow(width, height, "Vulkan Test", nullptr, nullptr);
		if (window == nullptr)
			return false;

		return true;
	}

	void PollInput() override
	{
	}

	U32 GetWidth() override
	{
		return width;
	}

	U32 GetHeight() override
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

	std::vector<const char*> GetRequiredExtensions(bool debugUtils) override
	{
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (debugUtils) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
	}
	
	std::vector<const char*> GetRequiredDeviceExtensions() override
	{
		return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}

	VkSurfaceKHR CreateSurface(VkInstance instance) override
	{
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(instance, GetWindow(), nullptr, &surface) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        int actual_width, actual_height;
        glfwGetFramebufferSize(GetWindow(), &actual_width, &actual_height);
        SetSize(
            unsigned(actual_width),
            unsigned(actual_height)
        );
        return surface;
	}

	void notify_close()
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		requestClose.store(true);
	}

	bool IsAlived()override
	{
		return !requestClose.load();
	}

	int RunMainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwWaitEvents();
			if (!asyncLoopAlive)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		return 0;
	}

	int RunAsyncLoop(App* app)
	{
		Profiler::SetThreadName("MainThread");
		asyncLoopAlive = true;
		mainLoop = std::thread(&PlatformGFLW::ThreadMainLoop, this, app);

		int ret = RunMainLoop();

		glfwSetWindowShouldClose(window, GLFW_TRUE);
		requestClose.store(true);

		if (mainLoop.joinable())
			mainLoop.join();

		return ret;
	}

	void ThreadMainLoop(App* app)
	{
		Profiler::SetThreadName("AsyncMainThread");
		app->Initialize();
		while (app->Poll())
			app->RunFrame();

		app->Uninitialize();
	}
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[])
{
	std::unique_ptr<App> app = std::unique_ptr<App>(createAppFunc(argc, argv));
    if (app == nullptr)
       return 1;

    std::unique_ptr<PlatformGFLW> platform = std::make_unique<PlatformGFLW>();
	auto* platformHandle = platform.get();
    if (!platform->Init(800, 600))
       return 1;

    if (!app->InitWSI(std::move(platform)))
       return 1;

	int ret = platformHandle->RunAsyncLoop(app.get());
    app.reset();
	return ret;
}
}