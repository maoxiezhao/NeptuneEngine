#pragma once

#include "renderPath2D.h"
#include "culling.h"
#include "renderScene.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderPath3D : public RenderPath2D
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;

		bool GetSceneUpdateEnabled()const {
			return sceneUpdateEnable;
		}

	protected:
		void SetupPasses(RenderGraph& renderGraph) override;
		void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override;
	
	private:
		bool sceneUpdateEnable = true;

		Visibility visibility;
		CameraComponent* camera = nullptr;
	};
}