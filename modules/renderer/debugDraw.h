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

		void DrawLine(const F32x3 p1, const F32x3 p2, const F32x4& color = F32x4(1, 1, 1, 1));
		void DrawBox(const FMat4x4& boxMatrix, const F32x4& color = F32x4(1, 1, 1, 1));
		void DrawSphere(const Sphere& sphere, const F32x4& color = F32x4(1, 1, 1, 1));
	}
}