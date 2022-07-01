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

		virtual void Update(float dt) {};
		virtual void FixedUpdate() {};
		virtual void Render() {};

		void SetWSI(WSI* wsi_) 
		{
			ASSERT(wsi_ != nullptr);
			wsi = wsi_;
		}

		void SetScene(RenderScene* scene_)  {
			scene = scene_;
		}

		RenderScene* GetScene() {
			return scene;
		}

	protected:
		WSI* wsi = nullptr;

	private:
		RenderScene* scene = nullptr;
	};
}