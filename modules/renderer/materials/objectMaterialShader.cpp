#include "objectMaterialShader.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	GPU::Shader* GetVSShader(RENDERPASS renderPass)
	{
		switch (renderPass)
		{
		case RENDERPASS_MAIN:
			return Renderer::GetShader(SHADERTYPE_OBJECT)->GetVS("VS", 0);
		case RENDERPASS_PREPASS:
			return Renderer::GetShader(SHADERTYPE_OBJECT)->GetVS("VS", 1);
		default:
			return nullptr;
		}
	}

	GPU::Shader* GetPSShader(RENDERPASS renderPass)
	{
		switch (renderPass)
		{
		case RENDERPASS_MAIN:
			return Renderer::GetShader(SHADERTYPE_OBJECT)->GetPS("PS", 0);
		case RENDERPASS_PREPASS:
			return Renderer::GetShader(SHADERTYPE_OBJECT)->GetPS("PS_Depth", 0);
		default:
			return nullptr;
		}
	}

	ObjectMaterialShader::ObjectMaterialShader(const String& name) :
		MaterialShader(name)
	{
	}

	ObjectMaterialShader::~ObjectMaterialShader()
	{
	}

	void ObjectMaterialShader::Bind(BindParameters& params)
	{
		GPU::CommandList& cmd = *params.cmd;
		auto cache = GetPSO(params.renderPass);
		ASSERT(cache != nullptr);
		cmd.SetPipelineState(cache->desc);
	}

	void ObjectMaterialShader::Unload()
	{
		MaterialShader::Unload();

		// TODO clear pipeline states
	}

	bool ObjectMaterialShader::OnLoaded()
	{
		// Load default object shader if current shader is null
		if (!shader)
		{
			shader = Renderer::GetShader(SHADERTYPE_OBJECT);
			if (!shader)
				return false;
		}

		// Create pipeline states
		drawModes = RENDERPASS_MAIN | RENDERPASS_PREPASS;

		RENDERPASS renderPasses[] = {RENDERPASS_MAIN, RENDERPASS_PREPASS};
		for (auto renderPass : renderPasses)
		{
			GPU::PipelineStateDesc pipeline = {};
			memset(&pipeline, 0, sizeof(pipeline));

			// Shaders
			const bool transparency = info.blendMode != BLENDMODE_OPAQUE;
			pipeline.shaders[(I32)GPU::ShaderStage::VS] = GetVSShader((RENDERPASS)renderPass);
			pipeline.shaders[(I32)GPU::ShaderStage::PS] = GetPSShader((RENDERPASS)renderPass);

			// Blend states
			switch (info.blendMode)
			{
			case BLENDMODE_OPAQUE:
				pipeline.blendState = Renderer::GetBlendState(BlendStateTypes::BSTYPE_OPAQUE);
				break;
			case BLENDMODE_ALPHA:
				pipeline.blendState = Renderer::GetBlendState(BlendStateTypes::BSTYPE_TRANSPARENT);
				break;
			case BLENDMODE_PREMULTIPLIED:
				pipeline.blendState = Renderer::GetBlendState(BlendStateTypes::BSTYPE_PREMULTIPLIED);
				break;
			default:
				ASSERT(false);
				break;
			}

			// DepthStencilStates
			switch (renderPass)
			{
			case RENDERPASS_MAIN:
				pipeline.depthStencilState = Renderer::GetDepthStencilState(DepthStencilStateType::DSTYPE_READEQUAL);
				break;
			default:
				pipeline.depthStencilState = Renderer::GetDepthStencilState(DepthStencilStateType::DSTYPE_DEFAULT);
				break;
			}

			// RasterizerStates
			switch (info.side)
			{
			case OBJECT_DOUBLESIDED_FRONTSIDE:
				pipeline.rasterizerState = Renderer::GetRasterizerState(RasterizerStateTypes::RSTYPE_FRONT);
				break;
			case OBJECT_DOUBLESIDED_ENABLED:
				pipeline.rasterizerState = Renderer::GetRasterizerState(RasterizerStateTypes::RSTYPE_DOUBLE_SIDED);
				break;
			case OBJECT_DOUBLESIDED_BACKSIDE:
				pipeline.rasterizerState = Renderer::GetRasterizerState(RasterizerStateTypes::RSTYPE_BACK);
				break;
			default:
				ASSERT(false);
				break;
			}

			switch (renderPass)
			{
			case VulkanTest::RENDERPASS_MAIN:
				defaultPSO.Init(pipeline);
				break;
			case VulkanTest::RENDERPASS_PREPASS:
				depthPSO.Init(pipeline);
				break;
			default:
				break;
			}
		}

		return true;
	}
}