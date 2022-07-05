#pragma once

#include "core\resource\resource.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Shader final : public Resource
	{
	public:
		DECLARE_RESOURCE(Shader);

		Shader(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Shader();

		GPU::ShaderProgramPtr GetProgram(const GPU::ShaderVariantMap& defines);

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;
	};
}