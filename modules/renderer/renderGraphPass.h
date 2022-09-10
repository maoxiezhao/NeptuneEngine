#pragma once

#include "renderGraph.h"
#include "core\memory\object.h"
#include "core\utils\singleton.h"

namespace VulkanTest
{
	class VULKAN_TEST_API RenderGraphPassBase : public Object
	{
	public:
		virtual bool Setup(RenderGraph& renderGraph) {
			return true;
		}

		virtual void Dispose() {
		}
	};

	template<class T>
	class RenderGraphPass : public RenderGraphPassBase, public Singleton<T>
	{
	};
}