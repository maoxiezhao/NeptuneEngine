#pragma once

#include "renderGraph.h"
#include "core\types\object.h"
#include "core\utils\singleton.h"

namespace VulkanTest
{
	class RenderPath3D;

	class VULKAN_TEST_API RenderGraphPassBase : public Object
	{
	public:
		virtual bool Init() {
			return true;
		}

		virtual void Dispose() {
			hasValidResources = false;
		}

		bool IsReady() {
			return CheckIfNeedPass();
		}

		virtual void Setup(RenderGraph& renderGraph, RenderPath3D& renderPath) {
		}

	protected:
		static bool DefaultClearColorFunc(U32 index, VkClearColorValue* value);
		static bool DefaultClearDepthFunc(VkClearDepthStencilValue* value);

	protected:
		bool CheckIfNeedPass()
		{
			if (hasValidResources)
				return true;

			const bool setupSuccess = SetupResources();
			hasValidResources = setupSuccess;
			return setupSuccess;
		}

		virtual bool SetupResources()
		{
			return true;
		}

		bool hasValidResources = false;
	};

	template<class T>
	class RenderGraphPass : public RenderGraphPassBase, public Singleton<T>
	{
	};
}