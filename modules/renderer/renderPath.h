#pragma once

#include "gpu\vulkan\device.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{
	class RenderScene;

	class VULKAN_TEST_API RenderPath
	{
	public:
		RenderPath() = default;
		virtual ~RenderPath() = default;

		virtual void Start() {};
		virtual void Stop() {};
		virtual void Update(float dt) {};
		virtual void FixedUpdate() {};
		virtual void Render() {};

		void SetWSI(WSI* wsi_) 
		{
			ASSERT(wsi_ != nullptr);
			wsi = wsi_;
		}

		void SetScene(RenderScene* scene_) 
		{
			// ASSERT(scene_ != nullptr);
			scene = scene_;
		}

	protected:
		WSI* wsi = nullptr;
		RenderScene* scene = nullptr;
	};
}