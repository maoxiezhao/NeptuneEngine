#pragma once

#include "renderPath2D.h"

namespace VulkanTest
{
	class RenderPath3D : public RenderPath2D
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;
		void Render() const override;
	};
}