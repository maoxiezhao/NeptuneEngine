#include "app\app.h"
#include "core\jobsystem\jobsystem.h"
#include "core\memory\memory.h"
#include "core\utils\profiler.h"
#include "core\platform\sync.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\wsi.h"
#include "GLFW\glfw3.h"

#include <thread>
#include <functional>

namespace VulkanTest
{
class PlatformGFLW;

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
static void WindowCloseCallback(GLFWwindow* window);

class PlatformGFLW : public WSIPlatform
{
public:
	struct Options
	{
		U32 overrideWidth = 0;
		U32 overrideHeight = 0;
		bool fullscreen = false;
	};

private:
    Options options;
	uint32_t width = 0;
	uint32_t height = 0;
	GLFWwindow* window = nullptr;
	bool asyncLoopAlive = true;
	std::atomic_bool requestClose;

private:
	struct EventList
	{
		Mutex mutex;
		std::vector<std::function<void()>> list;
	};
	EventList mainThreadEventList, asyncThreadEventList;

	void PorcessEventList(EventList& list)
	{
		ScopedMutex lock(list.mutex);
		for (auto& ent : list.list)
			ent();
		list.list.clear();
	}

	template<typename FUNC>
	void PushEventTaskToList(EventList& list, FUNC&& func)
	{
		ScopedMutex lock(list.mutex);
		list.list.emplace_back(std::forward<FUNC>(func));
	}

	template <typename FUNC>
	void PushEventTaskToMainThread(FUNC&& func)
	{
		PushEventTaskToList(mainThreadEventList, std::forward<FUNC>(func));
		glfwPostEmptyEvent();
	}
	
	template <typename FUNC>
	void PushEventTaskToAsyncThread(FUNC&& func)
	{
		PushEventTaskToList(asyncThreadEventList, std::forward<FUNC>(func));
	}

	void ProcessEventsMainThread()
	{
		PorcessEventList(mainThreadEventList);
	}

	void ProcessEventsAsyncThread()
	{
		PorcessEventList(asyncThreadEventList);
	}

public:
	PlatformGFLW(const Options &options_) :
        options(options_)
    {
    }

	virtual ~PlatformGFLW()
	{
		if (window != nullptr)
		{
			glfwDestroyWindow(window);
			glfwTerminate();
		}
	}

	bool Init(int width_, int height_, const char* title)
	{
		requestClose.store(false);
		width = width_;
		height = height_;

		if (options.overrideWidth > 0)
			width = options.overrideWidth;
		if (options.overrideHeight > 0)
			height = options.overrideHeight;

		if (!glfwInit())
		{
			Logger::Error("Failed to initialize GLFW");
			return false;
		}
		
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		// create window
		window = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (window == nullptr)
			return false;

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
		glfwSetWindowCloseCallback(window, WindowCloseCallback);
        glfwShowWindow(window);

		return true;
	}

	U32 GetWidth() override
	{
		return width;
	}

	U32 GetHeight() override
	{
		return height;
	}

	void PollInput() override
	{
		ProcessEventsAsyncThread();
	}

	void* GetWindow() override 
	{
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
        if (glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(GetWindow()), nullptr, &surface) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        int actual_width, actual_height;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(GetWindow()), &actual_width, &actual_height);
        SetSize(
            unsigned(actual_width),
            unsigned(actual_height)
        );
        return surface;
	}

	void NotifyResize(U32 width_, U32 height_)
	{
		PushEventTaskToAsyncThread([=]() {
			isResize = true;
			width = width_;
			height = height_;
		});
	}

	void NotifyClose()
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		requestClose.store(true);
	}

	bool IsAlived(WSI& wsi)override
	{
		ProcessEventsAsyncThread();
		return !requestClose.load();
	}

	int RunMainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwWaitEvents();
			ProcessEventsMainThread();
			if (!asyncLoopAlive)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		return 0;
	}

	int RunAsyncLoop(App* app)
	{
		Profiler::SetThreadName("MainThread");
		asyncLoopAlive = true;
		struct Data 
		{
			Data(App* app_, PlatformGFLW* platform_) :
				semaphore(0, 1),
				app(app_),
				platform(platform_)
			{}

			App* app;
			PlatformGFLW* platform;
			Semaphore semaphore;
		} data(app, this);

		// Async loop
		Jobsystem::Run(&data, [](void* ptr) {
			Profiler::SetThreadName("AsyncMainThread");
			Platform::SetCurrentThreadIndex(0);

			Data* data = static_cast<Data*>(ptr);
			data->app->Initialize();
			while (data->app->Poll()) {
				data->app->RunFrame();
			}
			data->app->Uninitialize();

			PlatformGFLW* platform = data->platform;
			platform->PushEventTaskToMainThread([platform]() { platform->asyncLoopAlive = false; });
		
			data->semaphore.Signal(1);
		}, nullptr);

		// Main loop
		while (!glfwWindowShouldClose(window))
		{
			glfwWaitEvents();
			ProcessEventsMainThread();
			if (!asyncLoopAlive)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
		NotifyClose();

		PROFILE_BLOCK("Sleeping");
		data.semaphore.Wait();

		return 0;
	}
};

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	auto* glfw = static_cast<PlatformGFLW*>(glfwGetWindowUserPointer(window));
	ASSERT(width != 0 && height != 0);
	glfw->NotifyResize(width, height);
}

static void WindowCloseCallback(GLFWwindow* window)
{
	auto* glfw = static_cast<PlatformGFLW*>(glfwGetWindowUserPointer(window));
	glfw->NotifyClose();
}

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[])
{
    PlatformGFLW::Options options = {};

	std::unique_ptr<App> app = std::unique_ptr<App>(createAppFunc(argc, argv));
    if (app == nullptr)
       return 1;

    std::unique_ptr<PlatformGFLW> platform = std::make_unique<PlatformGFLW>(options);
	auto* platformHandle = platform.get();
    if (!platform->Init(app->GetDefaultWidth(), app->GetDefaultHeight(), app->GetWindowTitle()))
       return 1;

	if (!app->InitializeWSI(std::move(platform)))
        return 1;

	int ret = platformHandle->RunAsyncLoop(app.get());
    app.reset();
	return ret;
}
}