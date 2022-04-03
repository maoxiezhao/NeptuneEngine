#pragma once

#include "gpu\vulkan\device.h"

namespace VulkanTest
{
	class RenderPath
	{
	public:
		RenderPath() = default;
		virtual ~RenderPath() = default;

		virtual void Start() {};
		virtual void Stop() {};
		virtual void Update(float dt) {};
		virtual void FixedUpdate() {};
		virtual void Render() const {};
		virtual void Compose(GPU::CommandList cmd)const {};
	};
}