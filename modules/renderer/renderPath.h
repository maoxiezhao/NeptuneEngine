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
		virtual void Compose(GPU::CommandList* cmd)const {};

		void SetDevice(GPU::DeviceVulkan* device_) {
			device = device_;
		}

		void SetPlatform(WSIPlatform* platform_) {
			platform = platform_;
		}

	protected:
		GPU::DeviceVulkan* device = nullptr;
		WSIPlatform* platform = nullptr;
	};
}