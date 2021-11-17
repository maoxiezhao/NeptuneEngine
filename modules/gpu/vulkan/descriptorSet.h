#pragma once

#include "definition.h"

namespace GPU
{
	enum class BindlessReosurceType
	{
		Image,
		Buffer,
		Sampler
	};

	struct DescriptorSetLayout
	{
		enum SetMask
		{
			SAMPLED_IMAGE,
			STORAGE_IMAGE,
			UNIFORM_BUFFER,
			STORAGE_BUFFER,
			SAMPLED_BUFFER,
			INPUT_ATTACHMENT,
			SAMPLER,
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
		DescriptorSetAllocator(DeviceVulkan& device_, const DescriptorSetLayout& layout, const U32* stageForBinds);
		~DescriptorSetAllocator();

		void BeginFrame();
		void Clear();

		VkDescriptorSetLayout GetSetLayout()
		{
			return setLayout;
		}

		std::pair<VkDescriptorSet, bool> GetOrAllocate(HashValue hash);

	private:
		DeviceVulkan& device;
		VkDescriptorSetLayout setLayout;
		std::vector<VkDescriptorPool> pools;
		std::vector<VkDescriptorPoolSize> poolSize;

		std::unordered_map<HashValue, VkDescriptorSet> setMap;
		std::vector<VkDescriptorSet> setVacants;
		VkDescriptorSet RequestVacant(HashValue hash);

		bool shouldBegin = false;
		bool isBindless = false;
	};
}