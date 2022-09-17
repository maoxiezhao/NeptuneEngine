#include "render2D.h"
#include "renderer\renderer.h"
#include "math\geometry.h"
#include "math\vMath_impl.hpp"
#include "renderer\render2D\fontResource.h"
#include "renderer\render2D\fontAltasTexture.h"
#include "renderer\render2D\fontManager.h"

#include "shaderInterop_font.h"

namespace VulkanTest
{
namespace Render2D
{
	struct FontVertex
	{
		F32x2 pos;
		F32x2 uv;
	};

	struct FontDrawCall
	{
		Texture* tex;
		U32 startIndex;
		U32 count;
	};

	static thread_local std::vector<FontDrawCall> drawCalls;
	static thread_local std::vector<FontVertex> fontVertexList;

	GPU::PipelineStateDesc psFont;
	ResPtr<Shader> fontShader;

	class Render2DService : public RendererService
	{
	public:
		Render2DService() :
			RendererService("Render2D", -600)
		{}

		bool Init(Engine& engine) override
		{
			auto& resManager = engine.GetResourceManager();
			fontShader = resManager.LoadResource<Shader>(Path("shaders/font.shd"));
			if (fontShader == nullptr)
			{
				Logger::Error("Failed to load font shader");
				return false;
			}

			// Rasterizer states
			GPU::RasterizerState rs;
			rs.fillMode = GPU::FILL_SOLID;
			rs.cullMode = VK_CULL_MODE_NONE;
			rs.frontCounterClockwise = true;
			rs.depthBias = 0;
			rs.depthBiasClamp = 0;
			rs.slopeScaledDepthBias = 0;
			rs.depthClipEnable = false;
			rs.multisampleEnable = false;
			rs.antialiasedLineEnable = false;
			rs.conservativeRasterizationEnable = false;
			psFont.rasterizerState = rs;

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
			psFont.blendState = bd;

			// Depth stencil state
			GPU::DepthStencilState dsd;
			dsd.depthEnable = false;
			dsd.stencilEnable = false;
			psFont.depthStencilState = dsd;

			// Topology
			psFont.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	
			initialized = true;
			return true;
		}

		void Uninit() override
		{
			fontShader.reset();
			initialized = false;
		}
	};
	Render2DService Render2DServiceInstance;

	void ParseText(Font* font, const String& text, const TextParmas& params, F32x2& textSize)
	{
		fontVertexList.clear();
		drawCalls.clear();

		F32 fontScale = 1.0f / Font::FontScale;
		F32x2 invAtlasSize = F32x2(1.0f);

		FontCharacterInfo charInfo;
		FontCharacterInfo previous;
		FontAltasTexture* fontAtlas = nullptr;
		I32 fontAltasIndex = -1;

		FontDrawCall* drawCall = nullptr;
		I32 quadCount = 0;
		
		// Convert stirng to wString for parsing now
		// TODO Use a commone StringView to replace it
		F32x3 pos = F32x3(0.0f); // params.position;
		textSize.x = 0.0f;
		textSize.y = pos.y + font->GetHeight() * fontScale;

		WString wText = StringToWString(text);
		for (U32 i = 0; i < wText.length(); i++)
		{
			const I32 curChar = wText[i];
			if (curChar != '\n')
			{
				font->GetCharacter(curChar, charInfo);

				// Get font altas texture
				if (fontAtlas == nullptr || fontAltasIndex != charInfo.textureIndex)
				{
					drawCall = &drawCalls.emplace_back();
					drawCall->startIndex = quadCount;
					drawCall->count = 0;

					fontAltasIndex = charInfo.textureIndex;
					fontAtlas = FontManager::GetAtlas(fontAltasIndex);
					if (fontAtlas)
					{
						fontAtlas->EnsureTextureCreated();
						invAtlasSize = F32x2(1.0f) / fontAtlas->GetSize();
						drawCall->tex = fontAtlas;
					}
					else
					{
						invAtlasSize = F32x2(1.0f);
						drawCall->tex = nullptr;
					}
				}

				I32 kerning;
				const bool isWhitespace = iswspace(curChar);
				if (!isWhitespace && previous.isValid)
				{
					kerning = font->GetKerning(previous.character, curChar);
				}
				else
				{
					kerning = 0;
				}
				pos.x += kerning * fontScale;
				previous = charInfo;

				if (!isWhitespace)
				{
					// Calculate character size and atlas coordinates
					const F32 x = pos.x + charInfo.offsetX * fontScale;
					const F32 y = pos.y + (font->GetHeight() + font->GetDescender() - charInfo.offsetY) * fontScale;
					Rectangle rect(x, y, charInfo.uvSize.x * fontScale, charInfo.uvSize.y * fontScale);

					F32x2 upperLeftUV = charInfo.uv * invAtlasSize;
					F32x2 rightBottomUV = (charInfo.uv + charInfo.uvSize) * invAtlasSize;

					const size_t vertexID = size_t(quadCount) * 4;
					fontVertexList.resize(vertexID + 4);
					quadCount++;
					drawCall->count++;

					//  3 --- 4
					//    \
					//	   \
					//      \
					//  2 --- 1

					fontVertexList[vertexID + 0] = { rect.GetUpperLeft(),   upperLeftUV };
					fontVertexList[vertexID + 1] = { rect.GetUpperRight(),  F32x2(rightBottomUV.x, upperLeftUV.y) };
					fontVertexList[vertexID + 2] = { rect.GetBottomLeft(),  F32x2(upperLeftUV.x, rightBottomUV.y)};
					fontVertexList[vertexID + 3] = { rect.GetBottomRight(), rightBottomUV };
				}

				// Next character
				pos.x += charInfo.advanceX * fontScale;
			}
			else
			{
				// New line
				pos.x = 0.0f;
				pos.y += font->GetHeight() * fontScale;
			}

			textSize.x = std::max(textSize.x, pos.x);
			textSize.y = std::max(textSize.y, pos.y + font->GetHeight() * fontScale);
		}
	}

	void DrawText(Font* font, const String& text, const TextParmas& params, GPU::CommandList& cmd)
	{
		GPU::DeviceVulkan* device = GPU::GPUDevice::Instance;
		if (font == nullptr || text.empty() || device == nullptr)
			return;

		if (!fontShader->IsReady())
			return;

		F32x2 textSize = F32x2(0.0f);
		ParseText(font, text, params, textSize);
		if (drawCalls.empty())
			return;

		auto allocation = cmd.AllocateStorageBuffer(fontVertexList.size() * sizeof(FontVertex));
		if (allocation.data == nullptr)
			return;

		memcpy(allocation.data, fontVertexList.data(), fontVertexList.size() * sizeof(FontVertex));

		FontCB fontCB = {};
		fontCB.bufferIndex = allocation.bindless->GetIndex();
		fontCB.bufferOffset = allocation.offset;
		fontCB.color = params.color.GetRGBA();

		F32x3 offset = F32x3(0.0f);
		if (params.hAligh == TextParmas::CENTER)
			offset.x -= textSize.x * 0.5f;
		if (params.vAligh == TextParmas::CENTER)
			offset.y += textSize.y * 0.5f;

		MATRIX M = MatrixTranslation(offset.x, offset.y, offset.z);
		M = M * MatrixScaling(params.scaling, params.scaling, params.scaling);
		if (params.customRotation)
			M = M * (*params.customRotation);

		M = M * MatrixTranslation(params.position.x, params.position.y, params.position.z);

		if (params.customProjection)
		{
			M = XMMatrixScaling(1, -1, 1) * M;	// screen projection is Y down, custom projection use world space
			M = M * (*params.customProjection);
		}
		else
		{
			// Create orthographic projection
			ASSERT(false);
		}

		fontCB.transform = StoreFMat4x4(M);

		for (FontDrawCall& drawcall : drawCalls)
		{
			if (drawcall.tex == nullptr ||
				drawcall.count <= 0)
				continue;

			cmd.BeginEvent("DrawText");

			fontCB.textureIndex = drawcall.tex->GetDescriptorIndex();
			cmd.BindConstant(fontCB, 0, 0);

			psFont.shaders[(U32)GPU::ShaderStage::VS] = fontShader->GetVS("VS");
			psFont.shaders[(U32)GPU::ShaderStage::PS] = fontShader->GetPS("PS");

			cmd.SetPipelineState(psFont);
			cmd.DrawInstanced(4, drawcall.count, 0, drawcall.startIndex);
			cmd.EndEvent();
		}
	}
}
}