#include "streamingHandler.h"

namespace VulkanTest
{
	I32 ModelsStreamingHandler::CalculateResidency(StreamableResource* resource)
	{
		return I32();
	}
	I32 ModelsStreamingHandler::CalculateRequestedResidency(StreamableResource* resource, I32 targetResidency)
	{
		return I32();
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

