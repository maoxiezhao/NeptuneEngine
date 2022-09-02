#pragma once

#include "core\common.h"
#include "core\jobsystem\task.h"

namespace VulkanTest
{
	class IStreamingHandler;

	// Streamable resource
	class VULKAN_TEST_API StreamableResource
	{
	public:
		StreamableResource(IStreamingHandler* streamingHandler_);
		~StreamableResource();

		void StartStreaming(bool isDynamic_);
		void StopStreaming();
		void CancleStreaming();

		virtual I32 GetMaxResidency() const = 0;
		virtual I32 GetCurrentResidency() const = 0;
	
		// Check current resource should be update
		virtual bool ShouldUpdate()const = 0;

		// Create streaming task
		virtual Task* CreateStreamingTask(I32 residency) = 0;

		// Requests the streaming update
		void RequestStreamingUpdate();

		IStreamingHandler* GetStreamingHandler() {
			return streamingHandler;
		}

		bool IsDynamic()const {
			return isDynamic;
		}

		// String infos
		I32 TargetResidency = 0;
		F32 LastUpdateTime = 0.0f;

	protected:
		IStreamingHandler* streamingHandler;
		bool isDynamic = true;
		bool isStreaming = false;
	};

	// Streaming main static class
	class Streaming
	{
	public:
		static void RequestStreamingUpdate();
	};
}