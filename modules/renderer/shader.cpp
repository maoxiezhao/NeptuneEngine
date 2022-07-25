#include "shader.h"
#include "renderer.h"
#include "core\scripts\luaConfig.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Shader);

	Shader::Shader(const Path& path_, RendererPlugin& renderer_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_),
		renderer(renderer_)
	{
	}

	Shader::~Shader()
	{
		ASSERT(IsEmpty());
	}

	void Shader::PreCompile(const GPU::ShaderVariantMap& defines)
	{
		ASSERT(shaderTemplate);

		HashCombiner hasher;
		for (auto& define : defines)
			hasher.HashCombine(define.c_str());

		auto it = programs.find(hasher.Get());
		if (!it.isValid())
			renderer.QueueShaderCompile(this, defines, shaderTemplate);
	}

	GPU::ShaderProgram* Shader::GetProgram(const GPU::ShaderVariantMap& defines)
	{
		ASSERT(shaderTemplate);

		HashCombiner hasher;
		for (auto& define : defines)
			hasher.HashCombine(define.c_str());
		auto it = programs.find(hasher.Get());
		if (it.isValid())
			return it.value();

		return renderer.QueueShaderCompile(this, defines, shaderTemplate);
	}

	void Shader::Compile(GPU::ShaderProgram*& program, const GPU::ShaderVariantMap& defines, GPU::ShaderTemplateProgram* shaderTemplate)
	{
		auto variant = shaderTemplate->RegisterVariant(defines);
		program = variant->GetProgram();
	}

	bool Shader::OnLoaded(U64 size, const U8* mem)
	{
		lua_State* rootState = renderer.GetEngine().GetLuaState();
		lua_State* l = lua_newthread(rootState);
		LuaConfig luaShader(l);
		luaShader.AddLightUserdata("this", this);

		const Span<const char> content((const char*)mem, (U32)size);
		if (!luaShader.Load(content))
			return false;

		BuildShaderTemplate();
		return true;
	}

	void Shader::OnUnLoaded()
	{
		shaderTemplate = nullptr;
		programs.clear();
	}

	void Shader::BuildShaderTemplate()
	{
		Array<GPU::ShaderMemoryData*> shaderDatas;
		for (auto& stage : sources.stages)
			shaderDatas.push_back(&stage);

		shaderTemplate = renderer.GetDevice()->GetShaderManager().RegsiterProgram(shaderDatas);
	}
}