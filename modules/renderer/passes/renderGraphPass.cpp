#include "renderGraphPass.h"

namespace VulkanTest
{
	bool RenderGraphPassBase::DefaultClearColorFunc(U32 index, VkClearColorValue* value)
	{
		if (value != nullptr)
		{
			value->float32[0] = 0.0f;
			value->float32[1] = 0.0f;
			value->float32[2] = 0.0f;
			value->float32[3] = 0.0f;
		}
		return true;
	}

	bool RenderGraphPassBase::DefaultClearDepthFunc(VkClearDepthStencilValue* value)
	{
		if (value != nullptr)
		{
			value->depth = 0.0f;
			value->stencil = 0;
		}
		return true;
	}
}