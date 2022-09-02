#pragma once

#include "core\common.h"
#include "core\collections\array.h"
#include "streaming.h"
#include "core\utils\singleton.h"

namespace VulkanTest
{
	class IStreamingHandler
	{
	public:
		virtual I32 CalculateResidency(StreamableResource* resource) = 0;
		virtual I32 CalculateRequestedResidency(StreamableResource* resource, I32 targetResidency) = 0;
	};

	class VULKAN_TEST_API ModelsStreamingHandler : public IStreamingHandler
	{
	public:
		I32 CalculateResidency(StreamableResource* resource) override;
		I32 CalculateRequestedResidency(StreamableResource* resource, I32 targetResidency) override;
	};

	class VULKAN_TEST_API StreamingHandlers : public Singleton<StreamingHandlers>
	{
	public:
		StreamingHandlers();
		~StreamingHandlers();

		IStreamingHandler* Model() {
			return model;
		}

	private:
		IStreamingHandler* model;
	};
}