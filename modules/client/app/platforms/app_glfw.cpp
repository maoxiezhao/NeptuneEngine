#include "app\app.h"
#include "gpu\vulkan\wsi.h"
#include "GLFW\glfw3.h"

namespace VulkanTest
{
class PlatformGFLW : public WSIPlatform
{
private:
	uint32_t width = 0;
	uint32_t height = 0;
	GLFWwindow* window = nullptr;

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

	bool Poll() override
	{
		glfwPollEvents();
    	return !glfwWindowShouldClose(window);
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

	int RunMainLoop(App* app)
	{
		while (app->Poll())
			app->Tick();

		return 0;
	}
};

int ApplicationMain(std::function<App*(int, char **)> createAppFunc, int argc, char *argv[])
{
	std::unique_ptr<App> app = std::unique_ptr<App>(createAppFunc(argc, argv));
    if (app == nullptr)
       return 1;

    std::unique_ptr<PlatformGFLW> platform = std::make_unique<PlatformGFLW>();
    if (!platform->Init(800, 600))
       return 1;

    if (!app->Initialize(std::move(platform)))
       return 1;

	int ret = platform->RunMainLoop(app.get());
    app->Uninitialize();
    app.reset();
	return ret;
}
}