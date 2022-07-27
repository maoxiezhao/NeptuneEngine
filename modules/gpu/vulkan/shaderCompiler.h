#pragma once

#include "definition.h"
#include "shader.h"

namespace VulkanTest
{
namespace GPU
{

namespace ShaderCompiler
{
	bool Preprocess();

	struct CompilerInput
	{
		ShaderStage stage = ShaderStage::Count;
		std::string shadersourcefilename;
		std::string entrypoint = "main";
		std::vector<std::string> includeDirectories;
		std::vector<ShaderMacro> defines;
		char* shadersourceData = nullptr;
		U32 shadersourceSize = 0;
	};
	struct CompilerOutput
	{
		const uint8_t* shaderdata = nullptr;
		size_t shadersize = 0;
		std::vector<uint8_t> shaderhash;
		std::string errorMessage;
		std::vector<std::string> dependencies;
		std::shared_ptr<void> internalState;
		inline bool IsValid() const { return internalState.get() != nullptr; }
	};

	void Initialize();
	bool Compile(const CompilerInput& input, CompilerOutput& output);
}

}
}