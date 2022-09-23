#include "sceneResource.h"

namespace VulkanTest
{
	REGISTER_JSON_RESOURCE(SceneResource);

	SceneResource::SceneResource(const ResourceInfo& info) :
		JsonResource(info)
	{
	}
		
	SceneResource::~SceneResource()
	{
	}
}