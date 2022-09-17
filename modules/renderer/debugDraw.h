#pragma once

#include "rendererCommon.h"
#include "renderer.h"

namespace VulkanTest
{
	class RenderScene;
	class ResourceManager;

	namespace DebugDraw
	{
		void DrawDebug(RenderScene& scene, const CameraComponent& camera, GPU::CommandList& cmd);

		void DrawBox(const FMat4x4& boxMatrix, const F32x4& color = F32x4(1, 1, 1, 1));
		void DrawSphere(const Sphere& sphere, const F32x4& color = F32x4(1, 1, 1, 1));
	}
}