#include "descriptorSet.h"
#include "device.h"
#include "image.h"

namespace GPU
{
	namespace 
	{
		static const unsigned VULKAN_NUM_SETS_PER_POOL = 16;

		static VkDescriptorType GetTypeBySetMask(DescriptorSetLayout::SetMask mask)
		{
			switch (mask)
			{
				case DescriptorSetLayout::SAMPLED_IMAGE:
					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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

		static U32 GetBindlessArraySize(DeviceVulkan& device, DescriptorSetLayout::SetMask mask)
		{
			auto& feature_1_2 = device.features.features_1_2;
			auto& properties_1_2 = device.features.properties_1_2;
			switch (mask)
			{
			case DescriptorSetLayout::SAMPLED_IMAGE:
				return feature_1_2.descriptorBindingUniformTexelBufferUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindSampledImages / 4 : -1;
			case DescriptorSetLayout::STORAGE_IMAGE:
				return feature_1_2.descriptorBindingStorageImageUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindStorageImages / 4 : -1;
			case DescriptorSetLayout::STORAGE_BUFFER:
				return feature_1_2.descriptorBindingStorageBufferUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindStorageBuffers / 4 : -1;
			case DescriptorSetLayout::SAMPLER:
				return feature_1_2.descriptorBindingSampledImageUpdateAfterBind ? 256 : -1;
			default:
				return -1;
				break;
			}
		}

		bool CheckSupportBindless(DeviceVulkan& device, const U32* maskFlags)
		{
			auto& feature_1_2 = device.features.features_1_2;
			bool ret = false;
			for (U32 maskbit = 0; maskbit < static_cast<U32>(DescriptorSetLayout::SetMask::COUNT); maskbit++)
			{
				if (maskFlags[maskbit] & 1)
				{
					DescriptorSetLayout::SetMask mask = static_cast<DescriptorSetLayout::SetMask>(maskbit);
					switch (mask)
					{
					case DescriptorSetLayout::SAMPLED_IMAGE:
						return feature_1_2.descriptorBindingUniformTexelBufferUpdateAfterBind;
					case DescriptorSetLayout::STORAGE_IMAGE:
						return feature_1_2.descriptorBindingStorageImageUpdateAfterBind;
					case DescriptorSetLayout::STORAGE_BUFFER:
						return feature_1_2.descriptorBindingStorageBufferUpdateAfterBind;
					case DescriptorSetLayout::SAMPLER:
						return feature_1_2.descriptorBindingSampledImageUpdateAfterBind;
					}
				}
			}
			return ret;
		}
	}

	void BindlessDescriptorPoolDeleter::operator()(BindlessDescriptorPool* buffer)
	{
		buffer->device.bindlessDescriptorPools.free(buffer);
	}

	BindlessDescriptorPool::~BindlessDescriptorPool()
	{
		if (pool != VK_NULL_HANDLE)
			device.ReleaseDescriptorPool(pool);
	}

	bool BindlessDescriptorPool::AllocateDescriptors(U32 count)
	{
		if (allocatedSets == totalSets ||
			allocatedDescriptors + count > totalDescriptors)
			return false;

		allocatedDescriptors += count;
		allocatedSets++;
		set = AllocateDescriptorSet(count);
		return set != VK_NULL_HANDLE;
	}

	void BindlessDescriptorPool::Reset()
	{
		if (set != VK_NULL_HANDLE)
		{
			ResetDescriptorSet(set);
			set = VK_NULL_HANDLE;
		}
		allocatedDescriptors  = 0;
		allocatedSets = 0;
	}

	VkDescriptorSet BindlessDescriptorPool::GetDescriptorSet() const
	{
		return set;
	}

	void BindlessDescriptorPool::SetTexture(int binding, const ImageView& iamgeView, VkImageLayout imageLayout)
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageView = iamgeView.GetImageView();
		imageInfo.imageLayout = imageLayout;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET  };
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.dstArrayElement = binding;
		write.descriptorCount = 1;
		write.dstSet = set;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device.device, 1, &write, 0, nullptr);
	}

	BindlessDescriptorPool::BindlessDescriptorPool(DeviceVulkan& device_, VkDescriptorPool pool_, DescriptorSetAllocator* allocator_, U32 totalSets_, U32 totalDescriptors_) :
		device(device_),
		pool(pool_),
		allocator(allocator_),
		totalSets(totalSets_),
		totalDescriptors(totalDescriptors_)
	{
	}

	VkDescriptorSet BindlessDescriptorPool::AllocateDescriptorSet(U32 numDescriptors)
	{
		VkDescriptorSetLayout setLayout = allocator->GetSetLayout();
		VkDescriptorSetAllocateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		info.descriptorPool = pool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &setLayout;

		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		if (vkAllocateDescriptorSets(device.device, &info, &descriptorSet) != VK_SUCCESS)
		{
			Logger::Warning("Failed to allocate descriptor set.");
			return VK_NULL_HANDLE;
		}

		return descriptorSet;
	}

	void BindlessDescriptorPool::ResetDescriptorSet(VkDescriptorSet descritprSet)
	{
		vkResetDescriptorPool(device.device, pool, 0);
	}

	DescriptorSetAllocator::DescriptorSetAllocator(DeviceVulkan& device_, const DescriptorSetLayout& layout, const U32* stageForBinds) :
		device(device_),
		layoutInfo(layout)
	{
		// check bindless enable
		isBindless = layout.arraySize[0] == DescriptorSetLayout::UNSIZED_ARRAY;
		if (isBindless && !CheckSupportBindless(device, layout.masks))
		{
			Logger::Error("Device does not support bindless descriptor allocate.");
			return;
		}

		VkDescriptorBindingFlagsEXT bindingFlags = 0;
		VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		if (isBindless)
		{
			info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			
			VkDescriptorBindingFlags bindingFlags =
				VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
				VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
				VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
		
			VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
			bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			bindingFlagsInfo.bindingCount = 1;
			bindingFlagsInfo.pBindingFlags = &bindingFlags;
			info.pNext = &bindingFlagsInfo;
		}

		std::vector<VkDescriptorSetLayoutBinding> bindings;
		for(U32 i = 0; i < VULKAN_NUM_BINDINGS; i++)
		{
			auto stages = stageForBinds[i];
			if (stages == 0)
				continue;
			
			// create VkDescriptorSetLayoutBinding
			U32 types = 0;
			for (U32 maskbit = 0; maskbit < static_cast<U32>(DescriptorSetLayout::SetMask::COUNT); maskbit++)
            {
				if (layout.masks[maskbit] & (1u << i))
				{
					// calculate array size and pool size
					U32 arraySize = layout.arraySize[i];
					U32 poolArraySize = 0;
					if (arraySize == DescriptorSetLayout::UNSIZED_ARRAY)
					{
						arraySize = GetBindlessArraySize(device, static_cast<DescriptorSetLayout::SetMask>(maskbit));
						if (arraySize <= 0)
						{
							Logger::Error("Device does not support bindless descriptor allocate.");
							return;
						}
						poolArraySize = arraySize;
					}
					else
					{
						poolArraySize = arraySize * VULKAN_NUM_SETS_PER_POOL;
					}

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

		// create descriptor set
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

	VkDescriptorPool DescriptorSetAllocator::AllocateBindlessPool(U32 numSets, U32 numDescriptors)
	{
		if (isBindless == false)
			return VK_NULL_HANDLE;

		VkDescriptorPoolSize size = poolSize[0];
		if (numDescriptors >= size.descriptorCount)
		{
			Logger::Warning("Allocate more than max bindless descriptors.");
			return VK_NULL_HANDLE;
		}
		size.descriptorCount = numDescriptors;

		VkDescriptorPoolCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		info.maxSets = numSets;
		info.poolSizeCount = 1;
		info.pPoolSizes = &size;

		VkDescriptorPool pool = VK_NULL_HANDLE;
		if (vkCreateDescriptorPool(device.device, &info, nullptr, &pool) != VK_SUCCESS)
		{
			Logger::Warning("Failed to create descriptor pool.");
			return VK_NULL_HANDLE;
		}
		return pool;
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