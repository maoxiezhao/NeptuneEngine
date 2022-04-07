#pragma once

#include "gpu\vulkan\device.h"
#include "gpu\vulkan\wsi.h"

namespace VulkanTest
{
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

		void SetWSI(WSI* wsi_) {
			wsi = wsi_;
		}

	protected:
		WSI* wsi = nullptr;
	};
}