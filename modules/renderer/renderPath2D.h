#pragma once

#include "renderPath.h"

namespace VulkanTest
{
	class RenderPath2D : public RenderPath
	{
	public:
		void Update(float dt) override;
		void FixedUpdate() override;
		void Render() const override;
	};
}