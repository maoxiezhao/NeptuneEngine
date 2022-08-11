#pragma once

#include "core\common.h"
#include "core\memory\memory.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Object
	{
	public:
		virtual ~Object();

		void DeleteObjectNow();
		void DeleteObject(F32 timeToRemove = 1.0f);

		virtual void Delete()
		{
			CJING_DELETE(this);
		}

		bool markedToDelete = false;
	};

	class VULKAN_TEST_API ObjectService
	{
	public:
		static void Dereference(Object* obj);
		static void Add(Object* obj, F32 time = 1.0f);
		static void Flush(F32 dt);

	private:
		static bool HasNewObjectsForFlush();
	};
}