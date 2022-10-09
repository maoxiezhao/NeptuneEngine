#pragma once

#include "shaderCompiler.h"

namespace VulkanTest
{
	class ShaderCompilerDX : public ShaderCompiler
	{
	public:
		ShaderCompilerDX();
		~ShaderCompilerDX();

	protected:
		bool CompileShader(ShaderFunctionMeta& meta) override;
	};
}