#pragma once

#include "core\resource\binaryResource.h"

namespace VulkanTest
{
	class VULKAN_TEST_API FontResource final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(FontResource);

#pragma pack(1)
		struct FontHeader
		{
			static constexpr U32 LAST_VERSION = 0;
			static constexpr U32 MAGIC = '_CST';
			U32 magic = MAGIC;
			U32 version = LAST_VERSION;
		};
#pragma pack()

		FontResource(const Path& path_, ResourceFactory& resFactory_);
		virtual ~FontResource();

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

	private:
		FontHeader header;
	};

	class VULKAN_TEST_API FontResourceFactory final : public BinaryResourceFactory
	{
	public:
		FontResourceFactory();
		virtual ~FontResourceFactory();

	protected:
		 Resource* CreateResource(const Path& path) override;
	};
}