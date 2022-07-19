#include "shaderSuite.h"

namespace VulkanTest
{
	void ShaderSuite::InitShaders(GPU::DeviceVulkan& device, const String& path, GPU::ShaderStage stage)
	{
	}

	void ShaderSuite::InitGraphics(GPU::DeviceVulkan& device, const String& vertex, const String& fragment)
	{
	}

	void ShaderSuite::InitCompute(GPU::DeviceVulkan& device, const String& compute)
	{
	}

	GPU::ShaderProgram* ShaderSuite::GetProgram(RENDERPASS renderPass, bool alphaTest)
	{
		return nullptr;
	}
}