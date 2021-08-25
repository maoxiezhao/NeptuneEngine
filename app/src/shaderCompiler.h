#pragma once

#include "gpu.h"

namespace ShaderCompiler
{
	void Initialize();
	bool LoadShader(DeviceVulkan& device, ShaderStage stage, Shader& shader, const std::string& filePath);
}