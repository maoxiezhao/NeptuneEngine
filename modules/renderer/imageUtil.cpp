#include "imageUtil.h"
#include "renderer.h"
#include "shaderInterop_image.h"
#include "content\resources\shader.h"
#include "content\resourceManager.h"

namespace VulkanTest::ImageUtil
{
    GPU::PipelineStateDesc pipelineStates
        [BLENDMODE_COUNT]
        [STENCILMODE_COUNT];
    ResPtr<Shader> shader;

    class ImageUtilService : public RendererService
    {
    public:
        ImageUtilService() :
            RendererService("ImageUtil", -800)
        {}

        bool Init(Engine& engine) override
        {
            shader = ResourceManager::LoadResource<Shader>(Path("shaders/image.shd"));

            GPU::BlendState blendStates[BLENDMODE_COUNT] = {};
            GPU::RasterizerState rasterizerState = {};
            GPU::DepthStencilState depthStencilStates[STENCILMODE_COUNT] = {};

            // Blend states
            GPU::BlendState bd;
            bd.renderTarget[0].blendEnable = true;
            bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
            bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
            bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
            bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
            bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
            bd.alphaToCoverageEnable = false;
            bd.independentBlendEnable = false;
            blendStates[BLENDMODE_ALPHA] = bd;

            bd.renderTarget[0].blendEnable = true;
            bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_ONE;
            bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
            bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
            bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
            bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
            blendStates[BLENDMODE_PREMULTIPLIED] = bd;

            bd.renderTarget[0].blendEnable = false;
            bd.renderTarget[0].renderTargetWriteMask = GPU::COLOR_WRITE_ENABLE_ALL;
            blendStates[BLENDMODE_OPAQUE] = bd;

            // Rasterizer states
            GPU::RasterizerState rs;
            rs.fillMode = GPU::FILL_SOLID;
            rs.cullMode = VK_CULL_MODE_NONE;
            rs.frontCounterClockwise = false;
            rs.depthBias = 0;
            rs.depthBiasClamp = 0;
            rs.slopeScaledDepthBias = 0;
            rs.depthClipEnable = true;
            rs.multisampleEnable = false;
            rs.antialiasedLineEnable = false;
            rs.conservativeRasterizationEnable = false;
            rasterizerState = rs;

            // DepthStencil states
            GPU::DepthStencilState dsd;
            dsd.depthEnable = false;
            dsd.stencilEnable = false;
            depthStencilStates[STENCILMODE_DISABLED] = dsd;

            dsd.stencilEnable = true;
            dsd.stencilReadMask = 0xFF;
            dsd.stencilWriteMask = 0;

            dsd.frontFace.stencilPassOp = VK_STENCIL_OP_KEEP;
            dsd.frontFace.stencilFailOp = VK_STENCIL_OP_KEEP;
            dsd.frontFace.stencilDepthFailOp = VK_STENCIL_OP_KEEP;
            dsd.backFace.stencilPassOp = VK_STENCIL_OP_KEEP;
            dsd.backFace.stencilFailOp = VK_STENCIL_OP_KEEP;
            dsd.backFace.stencilDepthFailOp = VK_STENCIL_OP_KEEP;

            dsd.frontFace.stencilFunc = VK_COMPARE_OP_EQUAL;
            dsd.backFace.stencilFunc = VK_COMPARE_OP_EQUAL;
            depthStencilStates[STENCILMODE_EQUAL] = dsd;

            dsd.frontFace.stencilFunc = VK_COMPARE_OP_LESS;
            dsd.backFace.stencilFunc = VK_COMPARE_OP_LESS;
            depthStencilStates[STENCILMODE_LESS] = dsd;

            dsd.frontFace.stencilFunc = VK_COMPARE_OP_GREATER;
            dsd.backFace.stencilFunc = VK_COMPARE_OP_GREATER;
            depthStencilStates[STENCILMODE_GREATER] = dsd;

            dsd.frontFace.stencilFunc = VK_COMPARE_OP_NOT_EQUAL;
            dsd.backFace.stencilFunc = VK_COMPARE_OP_NOT_EQUAL;
            depthStencilStates[STENCILMODE_NOT] = dsd;

            dsd.frontFace.stencilFunc = VK_COMPARE_OP_ALWAYS;
            dsd.backFace.stencilFunc = VK_COMPARE_OP_ALWAYS;
            depthStencilStates[STENCILMODE_ALWAYS] = dsd;

            GPU::PipelineStateDesc state = {};
            memset(&state, 0, sizeof(state));

            state.rasterizerState = rasterizerState;
            state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

            for (int j = 0; j < BLENDMODE_COUNT; ++j)
            {
                state.blendState = blendStates[j];
                for (int k = 0; k < STENCILMODE_COUNT; ++k)
                {
                    state.depthStencilState = depthStencilStates[k];
                    pipelineStates[j][k] = state;
                }
            }

            initialized = true;
            return true;
        }

        void Uninit() override
        {
            shader.reset();
            initialized = false;
        }
    };
    ImageUtilService ImageUtilServiceInstance;

    void Draw(GPU::Image* image, Params params, GPU::CommandList& cmd)
    {
        if (!shader->IsReady())
            return;

        ImageCB imageCB = {};
        imageCB.flags = 0;
        if (params.IsFullScreenEnabled())
            imageCB.flags |= IMAGE_FLAG_FULLSCREEN;

        // Calculate image corners
        MATRIX M = MatrixScaling(params.scale.x * params.siz.x, params.scale.y * params.siz.y, 1);
        M = M * MatrixRotationZ(params.rotation);
        M = M * MatrixTranslation(params.pos.x, params.pos.y, params.pos.z);

        XMVECTOR V = XMVectorSet(params.corners[0].x - params.pivot.x, params.corners[0].y - params.pivot.y, 0, 1);
        V = XMVector2Transform(V, M); // division by w will happen on GPU
        imageCB.corners0 = StoreF32x4(V);
        V = XMVectorSet(params.corners[1].x - params.pivot.x, params.corners[1].y - params.pivot.y, 0, 1);
        V = XMVector2Transform(V, M);
        imageCB.corners1 = StoreF32x4(V);
        V = XMVectorSet(params.corners[2].x - params.pivot.x, params.corners[2].y - params.pivot.y, 0, 1);
        V = XMVector2Transform(V, M);
        imageCB.corners2 = StoreF32x4(V);
        V = XMVectorSet(params.corners[3].x - params.pivot.x, params.corners[3].y - params.pivot.y, 0, 1);
        V = XMVector2Transform(V, M);
        imageCB.corners3 = StoreF32x4(V);

        cmd.BindConstant(imageCB, 0, CBSLOT_IMAGE);

        cmd.SetStencilRef(params.stencilRef, GPU::STENCIL_FACE_FRONT_AND_BACK);
        cmd.SetPipelineState(pipelineStates[params.blendFlag][params.stencilMode]);
        cmd.SetProgram(shader->GetVS("VS"), shader->GetPS("PS"));
        cmd.SetTexture(0, 0, image->GetImageView());
        cmd.SetSampler(0, 0, GPU::StockSampler::LinearClamp);
        cmd.Draw(3);
    }
}