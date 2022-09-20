#pragma once

#include "resource.h"
#include "storage\resourceStorage.h"
#include "resourceManager.h"
#include "core\serialization\iSerializable.h"
#include "core\serialization\json.h"

namespace VulkanTest
{
	typedef U16 AssetChunksFlag;

	class JsonResourceFactoryBase : public ResourceFactory
	{
	protected:
		Resource* NewResource(const ResourceInfo& info) override;
		Resource* CreateTemporaryResource(const ResourceInfo& info) override;
	};

	template <typename T>
	struct JsonResourceFactory : public JsonResourceFactoryBase
	{
	protected:
		virtual Resource* CreateResource(const ResourceInfo& info) override
		{
			return CJING_NEW(T)(info);
		}
	};

	class VULKAN_TEST_API JsonResourceBase : public Resource
	{
	public:
		virtual ~JsonResourceBase();

		bool IsReady()const;

		ISerializable::DeserializeStream* GetData() {
			return data;
		}

	protected:
		JsonResourceBase(const ResourceInfo& info);

		bool LoadResource() override;
		bool Load()override;
		void Unload() override;

	private:
		ISerializable::SerializeDocument document;
		ISerializable::DeserializeStream* data;
		String typeName;
	};

	class VULKAN_TEST_API JsonResource : public JsonResourceBase
	{
	public:
		virtual ~JsonResource();

	protected:
		JsonResource(const ResourceInfo& info);

		bool LoadResource() override;
		void Unload() override;

		virtual bool CreateInstance();
		void DeleteInstance();
	};
}