#pragma once

#include "core\common.h"
#include "core\platform\atomic.h"
#include "core\utils\threadLocal.h"

namespace VulkanTest
{
	struct RenderStats
	{
		volatile I64 drawCalls = 0;
		volatile I64 vertices = 0;
		volatile I64 triangles = 0;
		static volatile I32 enabled;

		static ThreadLocalObject<RenderStats> Counters;
		static RenderStats& GetCounter()
		{
			RenderStats*& stats = Counters.Get();
			if (stats == nullptr)
				stats = CJING_NEW(RenderStats);
			return *stats;
		}

		void Mix(const RenderStats& currentState)
		{
#define MIX(name) name = currentState.name - name
			MIX(drawCalls);
			MIX(vertices);
			MIX(triangles);
#undef MIX
		}

		void Max(const RenderStats& currentState)
		{
#define MIX(name) name = currentState.name + name
			MIX(drawCalls);
			MIX(vertices);
			MIX(triangles);
#undef MIX
		}
	};

#define RENDER_STAT_DRAW_CALL(vertices_, triangles_) \
	if (AtomicRead(&RenderStats::enabled) == 1) { \
	AtomicIncrement(&RenderStats::GetCounter().drawCalls); \
	AtomicAdd(&RenderStats::GetCounter().vertices, vertices_); \
	AtomicAdd(&RenderStats::GetCounter().triangles, triangles_);} 

#define ENABLE_RENDER_STAT() AtomicExchange(&RenderStats::enabled, 1);
#define DISABLE_RENDER_STAT() AtomicExchange(&RenderStats::enabled, 0);
}