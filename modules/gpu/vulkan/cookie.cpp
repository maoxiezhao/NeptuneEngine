#include "cookie.h"
#include "device.h"

namespace VulkanTest::GPU
{
	GraphicsCookie::GraphicsCookie(DeviceVulkan& device) :
		cookie(device.GenerateCookie())
	{
	}
}
