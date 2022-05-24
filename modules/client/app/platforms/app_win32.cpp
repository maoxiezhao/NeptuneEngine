#include "app\app.h"
#include "core\jobsystem\jobsystem.h"
#include "core\memory\memory.h"
#include "core\utils\profiler.h"
#include "core\platform\sync.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\wsi.h"

#include <thread>
#include <functional>

namespace VulkanTest
{

class PlatformWin32 : public WSIPlatform
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
	U32 width = 0;
	U32 height = 0;
	Platform::WindowType window = nullptr;
	bool asyncLoopAlive = true;
	std::atomic_bool requestClose;

	struct EventList
	{
		std::mutex lock;
		std::condition_variable cond;
		std::vector<std::function<void()>> list;
	};
	EventList mainThreadEventList, asyncThreadEventList;

	void PorcessEventList(EventList& list, bool blocking)
	{
		std::unique_lock<std::mutex> lock{ list.lock };
		if (blocking)
		{
			while (list.list.empty())
			{
				list.cond.wait(lock, [&list]() {
					return !list.list.empty();
					});
			}
		}

		for (auto& entTask : list.list) {
			entTask();
		}
		list.list.clear();
	}

	template<typename FUNC>
	void PushEventTaskToList(EventList& list, FUNC&& func)
	{
		std::lock_guard<std::mutex> lock{ list.lock };
		list.list.emplace_back(std::forward<FUNC>(func));
		list.cond.notify_one();
	}

	template <typename FUNC>
	void PushEventTaskToAsyncThread(FUNC&& func)
	{
		PushEventTaskToList(asyncThreadEventList, std::forward<FUNC>(func));
	}

	void ProcessEventsAsyncThreadBlocking()
	{
		PorcessEventList(asyncThreadEventList, true);
	}

public:
	PlatformWin32(const Options &options_) :
        options(options_)
    {
    }

	virtual ~PlatformWin32()
	{
		if (window != nullptr)
			Platform::DestroyCustomWindow(window);
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

		Platform::Initialize();
		Platform::LogPlatformInfo();

		Platform::WindowInitArgs args = {};
		args.name = title;
		args.width = width;
		args.height = height;
		window = Platform::CreateCustomWindow(args);
		if (window == Platform::INVALID_WINDOW)
		{
			Logger::Error("Failed to create main window");
			return false;
		}

		if (!GPU::VulkanContext::InitLoader(nullptr))
		{
			Logger::Error("Failed to initialize vulkan loader");
			return false;
		}

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

	Platform::WindowType GetWindow() override
	{
#ifdef CJING3D_PLATFORM_WIN32
		return window;
#endif
	}

	std::vector<const char*> GetRequiredExtensions(bool debugUtils) override
	{
        std::vector<const char*> extensions;
		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
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
		VkWin32SurfaceCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = window;
		createInfo.hinstance = GetModuleHandle(nullptr);

		VkResult res = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface);
		assert(res == VK_SUCCESS);
        return surface;
	}

	bool IsAlived(WSI& wsi)override
	{
		return !requestClose.load();
	}

	void BlockWSI(WSI& wsi)override
	{
		while (!resize && IsAlived(wsi))
			ProcessEventsAsyncThreadBlocking();
	}

	void NotifyResize(U32 width_, U32 height_)
	{
		if (width_ == width && height_ == height)
			return;

		PushEventTaskToAsyncThread([=]() {
			resize = true;
			width = width_;
			height = height_;
		});
	}
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[])
{
	std::unique_ptr<App> app = std::unique_ptr<App>(createAppFunc(argc, argv));
	if (app == nullptr)
		return 1;

	Profiler::SetThreadName("MainThread");
	struct Data
	{
		Data(App* app_) :
			semaphore(0, 1),
			app(app_)
		{}

		App* app;
		Semaphore semaphore;
	} 
	data(app.get());

	// Async loop
	Jobsystem::Run(&data, [](void* ptr) {
		Data* data = static_cast<Data*>(ptr);

		Profiler::SetThreadName("AsyncMainThread");
		Platform::SetCurrentThreadIndex(0);

		PlatformWin32::Options options = {};
		std::unique_ptr<PlatformWin32> platform = std::make_unique<PlatformWin32>(options);
		auto* platformHandle = platform.get();
		if (!platform->Init(data->app->GetDefaultWidth(), data->app->GetDefaultHeight(), data->app->GetWindowTitle()))
		{
			data->semaphore.Signal(1);
			return;
		}

		if (!data->app->InitializeWSI(std::move(platform)))
		{
			data->semaphore.Signal(1);
			return;
		}

		data->app->Initialize();
		while (data->app->Poll())
			data->app->RunFrame();
		data->app->Uninitialize();

		data->semaphore.Signal(1);
	}, nullptr);

	PROFILE_BLOCK("sleeping");
	data.semaphore.Wait();
	return 0;
}
}