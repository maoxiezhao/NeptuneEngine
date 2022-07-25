#pragma once

#include "core\resource\resource.h"
#include "core\collections\hashMap.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\shader.h"

namespace VulkanTest
{
	struct RendererPlugin;

	class VULKAN_TEST_API Shader final : public Resource
	{
	public:
		DECLARE_RESOURCE(Shader);

		struct Sources
		{
			Path path;
			Array<GPU::ShaderMemoryData> stages;
			String common;
		};

		Shader(const Path& path_, RendererPlugin& renderer_, ResourceFactory& resFactory_);
		virtual ~Shader();

		void PreCompile(const GPU::ShaderVariantMap& defines);
		GPU::ShaderProgram* GetProgram(const GPU::ShaderVariantMap& defines);

		static void Compile(GPU::ShaderProgram*& program, const GPU::ShaderVariantMap& defines, GPU::ShaderTemplateProgram* shaderTemplate);
		HashMap<U64, GPU::ShaderProgram*> programs;

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		void BuildShaderTemplate();

		RendererPlugin& renderer;
		GPU::ShaderTemplateProgram* shaderTemplate = nullptr;
		Sources sources;
	};
}