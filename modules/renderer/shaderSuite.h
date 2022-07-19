#pragma once

#include "gpu\vulkan\device.h"
#include "core\utils\string.h"
#include "renderer\enums.h"

namespace VulkanTest
{
	class VULKAN_TEST_API ShaderSuite
	{
	public:
		void InitShaders(GPU::DeviceVulkan& device, const String& path, GPU::ShaderStage stage);
		void InitGraphics(GPU::DeviceVulkan& device, const String& vertex, const String& fragment);
		void InitCompute(GPU::DeviceVulkan& device, const String& compute);

		GPU::ShaderProgram* GetProgram(RENDERPASS renderPass, bool alphaTest);
	};
}