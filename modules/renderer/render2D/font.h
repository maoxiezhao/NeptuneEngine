#pragma once

#include "fontResource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Font : public Object
	{
	public:
		Font(FontResource* res_, I32 size_);
		~Font();

		FontResource* GetResource() {
			return res;
		}

		I32 GetSize()const {
			return size;
		}

	private:
		I32 size;
		I32 height;
		FontResource* res;
	};
}