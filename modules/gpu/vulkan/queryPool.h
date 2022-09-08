#pragma once

#include "definition.h"
#include "buffer.h"

namespace VulkanTest
{
namespace GPU
{
	class DeviceVulkan;
	class QueryPool;
	class QueryPoolResult;
	class CommandList;

	enum QueryType
	{
		TIMESTAMP,
		OCCLUSION,
	};

	struct QueryPoolCreateDesc
	{
		QueryType queryType = QueryType::TIMESTAMP;
		U32 queryCount = 0;
	};

	struct QueryPoolResultDeleter
	{
		void operator()(QueryPoolResult* query);
	};
	class QueryPoolResult : public IntrusivePtrEnabled<QueryPoolResult, QueryPoolResultDeleter>
	{
	public:
		friend struct QueryPoolResultDeleter;

		void Signal(U64 time)
		{
			timestamp = time;
			hasTimestamp = true;
		}

		void Reset()
		{
			timestamp = 0;
			hasTimestamp = false;
		}

		U64 GetTimestamp() const
		{
			return timestamp;
		}

		bool HasTimesteamp() const 
		{
			return hasTimestamp;
		}

	private:
		friend class Util::ObjectPool<QueryPoolResult>;
		friend class QueryPool;

		explicit QueryPoolResult(DeviceVulkan& device_) :
			device(device_)
		{
		}

		DeviceVulkan& device;
		U64 timestamp = 0;
		bool hasTimestamp = false;
	};
	using QueryPoolResultPtr = IntrusivePtr<QueryPoolResult>;

	struct QueryPoolDeleter
	{
		void operator()(QueryPool* queryPool);
	};
	class QueryPool : public IntrusivePtrEnabled<QueryPool, QueryPoolDeleter>
	{
	public:
		QueryPool(DeviceVulkan& device_, const QueryPoolCreateDesc& desc);
		~QueryPool();

		void Reset(CommandList& cmd);
		void End(CommandList& cmd, I32 index);
		void Resolve(U32 index, U32 count, const Buffer* dest, U64 destOffset, CommandList& cmd);
		bool IsValid()const {
			return pool != VK_NULL_HANDLE;
		}

	private:
		friend struct QueryPoolDeleter;

		DeviceVulkan& device;
		VkQueryPool pool = VK_NULL_HANDLE;
		QueryType type;
		U32 count = 0;
	};
	using QueryPoolPtr = IntrusivePtr<QueryPool>;

	class TimestampQueryPool
	{
	public:
		TimestampQueryPool(DeviceVulkan& device_);
		~TimestampQueryPool();

		void Begin();
		QueryPoolResultPtr WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage);

	private:
		DeviceVulkan& device;
		VkQueryPool pool = VK_NULL_HANDLE;
		std::vector<U64> queryResults;
		std::vector<QueryPoolResultPtr> cookies;
		U32 index = 0;
		U32 size = 0;
	};
}
}