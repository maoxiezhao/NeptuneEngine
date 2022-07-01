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

		bool GetSceneUpdateEnabled()const {
			return sceneUpdateEnable;
		}

		void SetCamera(CameraComponent* camera_) {
			camera = camera_;
		}

		CameraComponent* GetCamera() {
			return camera;
		}

	protected:
		void SetupPasses(RenderGraph& renderGraph) override;
		void UpdateRenderData() override;
		void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override;
	
	protected:
		bool sceneUpdateEnable = true;

		Visibility visibility;
		CameraComponent* camera = nullptr;
	};
}