#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
namespace Editor
{
	namespace Gizmo
	{

		struct Config 
		{
			enum Mode 
			{
				TRANSLATE,
				ROTATE,
				SCALE
			} mode = TRANSLATE;

			bool IsTranslateMode() const { return mode == TRANSLATE; }
			bool IsRotateMode() const { return mode == ROTATE; }
			bool IsScaleMode() const { return mode == SCALE; }
		};

		void Draw(GPU::CommandList& cmd, CameraComponent& camera, const Config& config);
	}
}
}
