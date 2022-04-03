#pragma once

#include "core\common.h"

namespace VulkanTest
{
	namespace Profiler
	{
		void SetThreadName(const char* name);
		void BeginFrame();
		void EndFrame();
		void BeginBlock(const char* name);
		void EndBlock();

		struct Scope
		{
			explicit Scope(const char* name) { BeginBlock(name); }
			~Scope() { EndBlock(); }
		};
	}

#define PROFILER_CONCAT2(a, b) a ## b
#define PROFILER_CONCAT(a, b) PROFILER_CONCAT2(a, b)
#define PROFILE_BLOCK(name) Profiler::Scope PROFILER_CONCAT(profile_scope, __LINE__)(name);
}