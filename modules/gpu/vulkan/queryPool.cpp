#include "queryPool.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{
	void QueryPoolResultDeleter::operator()(QueryPoolResult* query)
	{
		query->device.queryPoolResults.free(query);
	}

	void QueryPoolDeleter::operator()(QueryPool* queryPool)
	{
		queryPool->device.queryPools.free(queryPool);
	}

	QueryPool::QueryPool(DeviceVulkan& device_, const QueryPoolCreateDesc& desc) :
		device(device_),
		type(desc.queryType),
		count(desc.queryCount)
	{
		VkQueryPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		poolInfo.queryCount = desc.queryCount;

		switch (desc.queryType)
		{
		case QueryType::TIMESTAMP:
			poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
			break;
		case QueryType::OCCLUSION:
			poolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
			break;
		}

		vkCreateQueryPool(device.device, &poolInfo, nullptr, &pool);
	}

	QueryPool::~QueryPool()
	{
		if (pool != VK_NULL_HANDLE)
			device.ReleaseQueryPoolNolock(pool);
	}

	void QueryPool::Reset(CommandList& cmd)
	{
		ASSERT(pool != VK_NULL_HANDLE);
		vkCmdResetQueryPool(
			cmd.GetCommandBuffer(),
			pool,
			0,
			count
		);
	}

	void QueryPool::End(CommandList& cmd, I32 index)
	{
		ASSERT(pool != VK_NULL_HANDLE);
		switch (type)
		{
		case QueryType::TIMESTAMP:
			vkCmdWriteTimestamp(cmd.GetCommandBuffer(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, pool, index);
			break;
		case QueryType::OCCLUSION:
			vkCmdEndQuery(cmd.GetCommandBuffer(), pool, index);
			break;
		}
	}

	void QueryPool::Resolve(U32 index, U32 count, const Buffer* dest, U64 destOffset, CommandList& cmd)
	{
		VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT |
			VK_QUERY_RESULT_WAIT_BIT;

		if (type == QueryType::OCCLUSION)
			flags |= VK_QUERY_RESULT_PARTIAL_BIT;

		vkCmdCopyQueryPoolResults(
			cmd.GetCommandBuffer(),
			pool,
			index,
			count,
			dest->GetBuffer(),
			destOffset,
			sizeof(uint64_t),
			flags
		);
	}

	TimestampQueryPool::TimestampQueryPool(DeviceVulkan& device_) :
		device(device_)
	{
		VkQueryPoolCreateInfo info = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		info.queryCount = 128;

		vkCreateQueryPool(device.device, &info, nullptr, &pool);
		size = info.queryCount;
		index = 0;
		queryResults.resize(info.queryCount);
		cookies.resize(info.queryCount);
	}

	TimestampQueryPool::~TimestampQueryPool()
	{
		if (pool != VK_NULL_HANDLE)
			vkDestroyQueryPool(device.device, pool, nullptr);
	}

	void TimestampQueryPool::Begin()
	{
		if (index != 0)
		{
			vkGetQueryPoolResults(device.device, pool,
				0, index,
				index * sizeof(uint64_t),
				queryResults.data(),
				sizeof(uint64_t),
				VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
		}

		for (unsigned j = 0; j < index; j++)
			cookies[j]->Signal(queryResults[j]);

		index = 0;
	}

	QueryPoolResultPtr TimestampQueryPool::WriteTimestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits stage)
	{
		auto cookie = QueryPoolResultPtr(device.queryPoolResults.allocate(device));
		cookies[index] = cookie;	
		vkCmdResetQueryPool(cmd, pool, index, 1);
		vkCmdWriteTimestamp(cmd, stage, pool, index);

		index++;
		return cookie;
	}
}
}