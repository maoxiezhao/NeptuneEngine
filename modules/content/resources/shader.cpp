#include "shader.h"
#include "core\scripts\luaConfig.h"
#include "core\profiler\profiler.h"

namespace VulkanTest
{
	REGISTER_BINARY_RESOURCE(Shader);

	Shader::Shader(const ResourceInfo& info) :
		ShaderResourceTypeBase<BinaryResource>(info)
	{
	}

	Shader::~Shader()
	{
		ASSERT(IsEmpty());
	}

	GPU::Shader* Shader::GetShader(GPU::ShaderStage stage, const String& name, I32 permutationIndex)
	{
		auto shader = shaders.Get(name, permutationIndex);
		if (shader == nullptr)
			Logger::Error("Missing shader %s %d", name.c_str(), permutationIndex);
		else if (shader->GetStage() != stage)
			Logger::Error("Invalid shader stage %s %d, Expected : %d, get %d", name.c_str(), permutationIndex, (U32)stage, (U32)shader->GetStage());
		
		return shader;
	}

	bool Shader::Load()
	{
		PROFILE_FUNCTION();

		ShaderCacheResult shaderCache;
		if (!LoadShaderCache(shaderCache))
		{
			Logger::Error("Failed to load shader cache %s", GetPath().c_str());
			return false;
		}

		InputMemoryStream shaderMem(shaderCache.Data);
		if (!shaders.LoadFromMemory(shaderMem))
		{
			Logger::Error("Failed to load shader %s", GetPath().c_str());
			return false;
		}

		return true;
	}

	void Shader::Unload()
	{
		shaders.Clear();
	}
}