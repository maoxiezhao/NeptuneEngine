#include "imageUtil.h"
#include "renderer.h"

namespace VulkanTest::ImageUtil
{
    void Draw(GPU::Image* image, Params params, GPU::CommandList& cmd)
    {
        cmd.SetDefaultTransparentState();
        cmd.SetBlendState(Renderer::GetBlendState(BSTYPE_TRANSPARENT));
        cmd.SetRasterizerState(Renderer::GetRasterizerState(RSTYPE_DOUBLE_SIDED));
        cmd.SetProgram("blitVS.hlsl", "blitPS.hlsl");
        cmd.SetTexture(0, 0, image->GetImageView());
        cmd.SetSampler(0, 0, GPU::StockSampler::NearestClamp);
        cmd.Draw(3);
    }
}