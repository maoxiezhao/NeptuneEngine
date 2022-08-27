#pragma once

#include "font.h"
#include "math\color.h"
#include "gpu\vulkan\device.h"
#include "renderer\shader.h"

namespace VulkanTest
{
namespace Render2D
{
	struct TextParmas
	{
		Color4 color = Color4::White();
		F32x3 position = F32x3(0.0f);
		F32 scaling = 1.0f;

		enum Alignment
		{
			LEFT,		// left alignment (horizontal)
			CENTER,		// center alignment (horizontal or vertical)
			RIGHT,		// right alignment (horizontal)
			TOP,		// top alignment (vertical)
			BOTTOM		// bottom alignment (vertical)
		};
		Alignment hAligh = LEFT;
		Alignment vAligh = TOP;
	
		const MATRIX* customProjection = nullptr;
		const MATRIX* customRotation = nullptr;
	};

	void Initialize(ResourceManager* resourceManager);
	void Uninitialize();

	void DrawText(Font* font, const String& text, const TextParmas& params, GPU::CommandList& cmd);
}
}