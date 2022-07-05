#include "shader.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Shader);

	Shader::Shader(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Shader::~Shader()
	{
	}

	GPU::ShaderProgramPtr Shader::GetProgram(const GPU::ShaderVariantMap& defines)
	{
		return GPU::ShaderProgramPtr();
	}

	bool Shader::OnLoaded(U64 size, const U8* mem)
	{
		return false;
	}

	void Shader::OnUnLoaded()
	{
	}
}