#include "streamingHandler.h"

namespace VulkanTest
{
	I32 ModelsStreamingHandler::CalculateResidency(StreamableResource* resource)
	{
		// TODO Cacluclate residency by target quality
		const I32 maxResidency = resource->GetMaxResidency();
		return maxResidency;
	}
	I32 ModelsStreamingHandler::CalculateRequestedResidency(StreamableResource* resource, I32 targetResidency)
	{
		I32 residency = targetResidency;
		I32 currentResidency = resource->GetCurrentResidency();
		if (currentResidency < targetResidency)
			residency = currentResidency + 1;
		else if (currentResidency > targetResidency)
			residency = currentResidency - 1;

		return residency;
	}

	StreamingHandlers::StreamingHandlers()
	{
		model = CJING_NEW(ModelsStreamingHandler)();
	}

	StreamingHandlers::~StreamingHandlers()
	{
		CJING_SAFE_DELETE(model);
	}
}

