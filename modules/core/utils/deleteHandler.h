#pragma once

#include "core\memory\memory.h"

namespace VulkanTest 
{
	template<typename T>
	class DeleteHandler
	{
	public:
        explicit DeleteHandler(T* toDelete_)
            : toDelete(toDelete_)
        {
        }

        ~DeleteHandler()
        {
            CJING_SAFE_DELETE(toDelete);
        }

    private:
        T* toDelete = nullptr;
	};
}