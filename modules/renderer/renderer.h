#pragma once

#include "enums.h"
#include "gpu\vulkan\device.h"
#include "core\plugin\plugin.h"
#include "culling.h"

namespace VulkanTest
{
	class RenderPath;
	class RenderScene;

	struct VULKAN_TEST_API RendererPlugin : public IPlugin
	{
		virtual void ActivePath(RenderPath* renderPath) = 0;
		virtual void Render() = 0;
	};

	namespace Renderer
	{
		void Initialize(Engine& engine);
		void Uninitialize();

		const GPU::BlendState& GetBlendState(BlendStateTypes types);
		const GPU::RasterizerState& GetRasterizerState(RasterizerStateTypes types);

		enum class RenderQueueType
		{
			Opaque = 1 << 0,
			Transparent = 1 << 1,
			All = Opaque | Transparent
		};

		struct RenderBatch;
		using RenderFunc = void (*)(GPU::CommandList&, const RenderBatch*, unsigned);

		struct VULKAN_TEST_API RenderBatch
		{
			RenderFunc renderFunc;
			U64 sortingKey;
		};

		struct VULKAN_TEST_API RenderQueue
		{
			RenderQueue() = default;
			void operator=(const RenderQueue&) = delete;
			RenderQueue(const RenderQueue&) = delete;
			~RenderQueue();

			void Sort();
			void Clear();
			bool Empty()const;
			size_t Size()const;
		};

		void UpdateFrameData(const Visibility& visible, RenderScene& scene, F32 delta);
		void UpdateRenderData(const Visibility& visible, GPU::CommandList& cmd);

		void DrawScene(GPU::CommandList& cmd);
		void DrawMeshes(GPU::CommandList& cmd, const Visibility& visible, const RenderQueue& queue, RENDERPASS renderPass, U32 renderFlags);

		// Plugin interface
		RendererPlugin* CreatePlugin(Engine& engine);
	}
}