#pragma once

#include "gpu.h"

namespace Renderer
{
	bool LoadShader(ShaderStage stage, Shader& shader, const std::string& filePath);
}