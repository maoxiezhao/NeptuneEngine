#pragma once

#include "editor\common.h"
#include "core\platform\platform.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{
namespace Editor
{
	class SplashScreen
	{
	public:
		SplashScreen();
		~SplashScreen();

		void Show(WSI& mainWSI);
		void Close();
		void Render();

	private:
		Platform::WindowType window = nullptr;
		WSI wsi;
	};
}
}