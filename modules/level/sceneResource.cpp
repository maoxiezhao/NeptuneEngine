#include "sceneResource.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(SceneResource);

	SceneResource::SceneResource(const ResourceInfo& info) :
		JsonResource(info)
	{
	}
		
	SceneResource::~SceneResource()
	{
	}
}