#include "scriptingObject.h"

namespace VulkanTest
{
	ScriptingObject::ScriptingObject(const ScriptingObjectParams& params) :
		guid(params.guid)
	{
	}

	ScriptingObject::~ScriptingObject()
	{
	}
}