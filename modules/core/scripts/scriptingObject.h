#pragma once

#include "core\memory\object.h"
#include "core\utils\guid.h"

namespace VulkanTest
{
	struct ScriptingObjectParams
	{
		Guid guid;

		FORCE_INLINE ScriptingObjectParams(const Guid& id) : guid(id){}
	};

	class VULKAN_TEST_API ScriptingObject : public Object
	{
	protected:
		Guid guid;

	public:
		explicit ScriptingObject(const ScriptingObjectParams& params);
		virtual ~ScriptingObject();
	
		FORCE_INLINE const Guid& GetGUID() const {
			return guid;
		}
	};

#define DECLARE_SCRIPTING_TYPE(type) \
    explicit type() : type(ScriptingObjectParams(Guid::New())) { } \
    explicit type(const ScriptingObjectParams& params)

}