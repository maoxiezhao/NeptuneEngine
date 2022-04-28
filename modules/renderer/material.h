#pragma once

#include "core\resource\resource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Material final : public Resource
	{
	public:
		DECLARE_RESOURCE(Material);

		Material(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Material();
	
	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;
	};
}