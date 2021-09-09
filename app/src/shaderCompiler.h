#pragma once

#include "vulkan/device.h"

namespace ShaderCompiler
{
	void Initialize();
	bool LoadShader(DeviceVulkan& device, ShaderStage stage, Shader& shader, const std::string& filePath);
}