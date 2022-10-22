#include "splashScreen.h"
#include "renderer\imageUtil.h"
#include "renderer\textureHelper.h"

namespace VulkanTest
{
namespace Editor
{
	SplashScreen::SplashScreen() :
		window(nullptr)
	{

	}

	SplashScreen::~SplashScreen()
	{
		Close();
	}

	void SplashScreen::Show(WSI& mainWSI)
	{
		auto desktopSize = Platform::GetDesktopSize();
		F32 dpiScale = (F32)Platform::GetDPI() / (F32)Platform::GetDefaultDPI();
		Platform::WindowInitArgs args = {};
		args.name = "Editor";
		args.width = (U32)(500.0f * dpiScale);
		args.height = (U32)(170.0f * dpiScale);
		args.posX = (U32)((desktopSize.x - args.width) * 0.5f);
		args.posY = (U32)((desktopSize.y - args.height) * 0.5f);
		args.flags = Platform::WindowInitArgs::NO_TASKBAR_ICON | Platform::WindowInitArgs::NO_DECORATION;
		args.hasBorder = false;
		args.showWindow = false;
		window = Platform::CreateCustomWindow(args);
		if (window == Platform::INVALID_WINDOW)
		{
			Logger::Error("Failed to create splash window");
			return;
		}

		Platform::ShowCustomWindow(window);

		WSIPlatform* platform = mainWSI.GetPlatform();
		GPU::VulkanContext* context = mainWSI.GetContext();
		VkSurfaceKHR surface = platform->CreateSurface(context->GetInstance(), (Platform::WindowType)window);
		ASSERT(surface != VK_NULL_HANDLE);

		auto clentBounds = Platform::GetClientBounds(window);
		bool ret = wsi.InitializeExternal(surface, *mainWSI.GetDevice(), *context, (I32)clentBounds.width, (I32)clentBounds.height);
		ASSERT(ret);
	}

	void SplashScreen::Close()
	{
		if (window)
			Platform::DestroyCustomWindow(window);

		wsi.Uninitialize();
	}

	void SplashScreen::Render()
	{
		GPU::DeviceVulkan* device = wsi.GetDevice();
		wsi.PresentBegin();
		{
			GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(&wsi.GetSwapchain(), GPU::SwapchainRenderPassType::ColorOnly);
			GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
			cmd->BeginRenderPass(rp);

			cmd->EndRenderPass();
			device->Submit(cmd);
		}
		wsi.PresentEnd();
	}
}
}