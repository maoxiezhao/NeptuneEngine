#pragma once

#include "core\resource\resource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Mesh
	{

	};

	class VULKAN_TEST_API Model final : public Resource
	{
	public:
		DECLARE_RESOURCE(Model);

		Model(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Model();
	
	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;
	};
}