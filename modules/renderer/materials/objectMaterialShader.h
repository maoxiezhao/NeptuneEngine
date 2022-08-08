#pragma once

#include "materialShader.h"

namespace VulkanTest
{
	struct MaterialFactory;

	class VULKAN_TEST_API ObjectMaterialShader : public MaterialShader
	{
	public:
		ObjectMaterialShader(const String& name);
		~ObjectMaterialShader();

		void Bind(BindParameters& params) override;
		void Unload() override;

	protected:
		bool OnLoaded() override;

	private:
		struct ObjectPipelineStateCache
		{
			GPU::PipelineStateDesc desc;

			void Init(GPU::PipelineStateDesc desc_)
			{
				desc = desc_;
			}
		};

		ObjectPipelineStateCache defaultPSO;
		ObjectPipelineStateCache depthPSO;

		ObjectPipelineStateCache* GetPSO(RENDERPASS pass)
		{
			switch (pass)
			{
			case VulkanTest::RENDERPASS_MAIN:
				return &defaultPSO;
			case VulkanTest::RENDERPASS_PREPASS:
				return &depthPSO;
			default:
				return nullptr;
			}
		}
		U32 drawModes = 0;
	};
}