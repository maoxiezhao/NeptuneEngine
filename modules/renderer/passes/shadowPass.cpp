#include "ShadowPass.h"
#include "renderer.h"
#include "renderPath3D.h"
#include "renderer.h"
#include "renderPath3D.h"

namespace VulkanTest
{
	bool ShadowPass::Init()
	{
		return true;
	}

	void ShadowPass::Dispose()
	{
		shader.reset();
	}

	void ShadowPass::Setup(RenderGraph& renderGraph, RenderPath3D& renderPath)
	{
		AttachmentInfo rtAttachment = renderPath.GetAttachmentRT();
	}
}
