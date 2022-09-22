#pragma once

#include "resource.h"
#include "storage\resourceStorage.h"
#include "resourceManager.h"

namespace VulkanTest
{
	typedef U16 AssetChunksFlag;

	class VULKAN_TEST_API BinaryResourceFactoryBase : public ResourceFactory
	{
	protected:
		Resource* NewResource(const ResourceInfo& info) override;
		Resource* CreateTemporaryResource(const ResourceInfo& info) override;
	};

	template <typename T>
	struct BinaryResourceFactory : public BinaryResourceFactoryBase
	{
	protected:
		virtual Resource* CreateResource(const ResourceInfo& info) override
		{
			return CJING_NEW(T)(info);
		}
	};

#define REGISTER_BINARY_RESOURCE(type)	\
	DEFINE_RESOURCE(type);	\
	class CONCAT_MACROS(ResourceFactory, type) : public BinaryResourceFactory<type> \
	{ \
	public: \
		CONCAT_MACROS(ResourceFactory, type)() { Initialize(type::ResType); } \
		~CONCAT_MACROS(ResourceFactory, type)() { Uninitialize(); } \
	}; \
	static CONCAT_MACROS(ResourceFactory, type) CONCAT_MACROS(CResourceFactory, type)
}