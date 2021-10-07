#include "descriptorSet.h"
#include "device.h"

namespace GPU
{
	static const unsigned VULKAN_NUM_SETS_PER_POOL = 16;

	static VkDescriptorType GetTypeBySetMask(DescriptorSetLayout::SetMask mask)
	{
		switch (mask)
		{
		case DescriptorSetLayout::SAMPLED_IMAGE:
				return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case DescriptorSetLayout::STORAGE_IMAGE:
				return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			case DescriptorSetLayout::UNIFORM_BUFFER:
				return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			case DescriptorSetLayout::STORAGE_BUFFER:
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case DescriptorSetLayout::SAMPLED_BUFFER:
				return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			case DescriptorSetLayout::INPUT_ATTACHMENT:
				return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			case DescriptorSetLayout::SAMPLER:
				return VK_DESCRIPTOR_TYPE_SAMPLER;
			default:
				assert(false);
				break;
		}
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	}

	DescriptorSetAllocator::DescriptorSetAllocator(DeviceVulkan& device_, const DescriptorSetLayout& layout, const U32* stageForBinds) :
		device(device_)
	{
		VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		// loop bindings
		for(U32 i = 0; i < VULKAN_NUM_BINDINGS; i++)
		{
			auto stages = stageForBinds[i];
			if (stages == 0)
				continue;

			// calculate array size and pool size
			U32 arraySize = layout.arraySize[i];
			U32 poolArraySize = 0;
			if (arraySize == DescriptorSetLayout::UNSIZED_ARRAY)
			{
				arraySize = VULKAN_NUM_BINDINGS_BINDLESS_VARYING;
				poolArraySize = arraySize;
			}
			else
			{
				poolArraySize = arraySize * VULKAN_NUM_SETS_PER_POOL;
			}
			
			// create VkDescriptorSetLayoutBinding
			U32 types = 0;
			for (U32 maskbit = 0; i < static_cast<U32>(DescriptorSetLayout::SetMask::COUNT); maskbit++)
            {
				if (layout.masks[maskbit] & (1 << i))
				{
					auto descriptorType = GetTypeBySetMask(static_cast<DescriptorSetLayout::SetMask>(maskbit));
					bindings.push_back({
						i, 										// binding
						descriptorType, 						// descriptorType
						arraySize, 								// descriptorCount
						stages, 								// stageFlags
						nullptr 								// pImmutableSamplers
					});
					poolSize.push_back({ descriptorType, poolArraySize });
					types++;
				}
			}	
			assert(types <= 1);
		}

		if (!bindings.empty())
		{
			info.bindingCount = (U32)bindings.size();
			info.pBindings = bindings.data();
		}

		if (vkCreateDescriptorSetLayout(device.device, &info, nullptr, &setLayout) != VK_SUCCESS)
		{
			Logger::Error("Failed to create descriptor set layout.");
		}
	}

	DescriptorSetAllocator::~DescriptorSetAllocator()
	{
		if (setLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device.device, setLayout, nullptr);

		Clear();
	}

	void DescriptorSetAllocator::BeginFrame()
	{
		shouldBegin = true;
	}

	void DescriptorSetAllocator::Clear()
	{
		for(auto& pool : pools)
		{
			vkResetDescriptorPool(device.device, pool, 0);
			vkDestroyDescriptorPool(device.device, pool, nullptr);
		}
		pools.clear();
	}

	std::pair<VkDescriptorSet, bool> DescriptorSetAllocator::GetOrAllocate(HashValue hash)
	{
		// free set map and push them into setVacants
		if (shouldBegin)
		{
			shouldBegin = false;
			for(auto kvp : setMap)
			{
				setVacants.push_back(kvp.second);
			}
			setMap.clear();
		}

		auto it = setMap.find(hash);
		if (it != setMap.end())
			return { it->second, true };
		
		auto set = RequestVacant(hash);
		if (set != VK_NULL_HANDLE)
			return { set, false };

		// create descriptor pool
		VkDescriptorPool pool;
		VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		info.maxSets = VULKAN_NUM_SETS_PER_POOL;
		if (!poolSize.empty())
		{
			info.poolSizeCount = (U32)poolSize.size();
			info.pPoolSizes = poolSize.data();
		}

		if (vkCreateDescriptorPool(device.device, &info, nullptr, &pool) != VK_SUCCESS)
		{
			return { VK_NULL_HANDLE, false };
		}

		// Create descriptor set
		// 一次性分配VULKAN_NUM_SETS_PER_POOL个descriptor set并缓存起来，以减少分配的次数
		VkDescriptorSet sets[VULKAN_NUM_SETS_PER_POOL];
		VkDescriptorSetLayout layouts[VULKAN_NUM_SETS_PER_POOL];
		std::fill(std::begin(layouts), std::end(layouts), setLayout);

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = VULKAN_NUM_SETS_PER_POOL;
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(device.device, &allocInfo, sets) != VK_SUCCESS)
		{
			Logger::Error("Failed to allocate descriptor sets.");
			return { VK_NULL_HANDLE, false };
		}

		// 缓存sets，并分配一个set返回
		pools.push_back(pool);
		for(auto set : sets)
			setVacants.push_back(set);

		return { RequestVacant(hash), false };
	}

	VkDescriptorSet DescriptorSetAllocator::RequestVacant(HashValue hash)
	{
		if (setVacants.empty())
			return VK_NULL_HANDLE;
		
		auto top = setVacants.back();
		setVacants.pop_back();
		setMap.insert(std::make_pair(hash, top));
		return top;
	}
}