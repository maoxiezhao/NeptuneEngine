#pragma once

#include "definition.h"

namespace GPU
{
	struct DescriptorSetLayout
	{
		enum SetMask
		{
			SAMPLED_IMAGE,
			STORAGE_IMAGE,
			uniform_buffer,
			STORAGE_BUFFER,
			SAMPLED_BUFFER,
			INPUT_ATTACHMENT,
			SAMPLER,
			SEPARATE_IMAGE,
			FP,
			IMMUTABLE_SAMPLER,
			COUNT,
		};
		U32 masks[static_cast<U32>(SetMask::COUNT)] = {};
		uint8_t arraySize[VULKAN_NUM_BINDINGS] = {};
		enum { UNSIZED_ARRAY = 0xff };

		DescriptorSetLayout()
		{
			memset(masks, 0, sizeof(U32) * static_cast<U32>(SetMask::COUNT));
		}
	};

	class DescriptorSetAllocator : public HashedObject<DescriptorSetAllocator>
	{
	public:
		DescriptorSetAllocator(DeviceVulkan& device_);
		~DescriptorSetAllocator();

		void BeginFrame();

		VkDescriptorSetLayout GetSetLayout()
		{
			return setLayout;
		}

		std::pair<VkDescriptorSet, bool> FindSet(HashValue hash);

	private:
		DeviceVulkan& device;
		VkDescriptorSetLayout setLayout;
		std::vector<VkDescriptorPool> pools;
		std::vector<VkDescriptorPoolSize> poolSize;
		bool isBindLess = false;
	};
}