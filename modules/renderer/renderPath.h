#pragma once

#include "gpu\vulkan\device.h"

namespace VulkanTest
{
	class RenderPath
	{
	public:
		RenderPath() = default;
		virtual ~RenderPath() = default;

		virtual void Update(float dt) = 0;
		virtual void FixedUpdate()  = 0;
		virtual void Render() const = 0;
	};
}