#pragma once

#include "core\resource\resource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API Texture final : public Resource
	{
	public:
		DECLARE_RESOURCE(Texture);

		Texture(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Texture();

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;
	};
}