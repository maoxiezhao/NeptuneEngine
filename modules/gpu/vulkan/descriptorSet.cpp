#include "descriptorSet.h"
#include "device.h"
#include "image.h"

namespace VulkanTest
{
namespace GPU
{
	namespace 
	{
		//DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE,
		//DESCRIPTOR_SET_TYPE_STORAGE_IMAGE,
		//DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER,
		//DESCRIPTOR_SET_TYPE_STORAGE_BUFFER,
		//DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER,
		//DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT,
		//DESCRIPTOR_SET_TYPE_SAMPLER,
		//DESCRIPTOR_SET_TYPE_COUNT,

		static const unsigned VULKAN_NUM_SETS_PER_POOL = 16;

		static VkDescriptorType GetTypeBySetMask(DescriptorSetType mask)
		{
			switch (mask)
			{
				case DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE:
					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				case DESCRIPTOR_SET_TYPE_STORAGE_IMAGE:
					return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				case DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER:
					return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				case DESCRIPTOR_SET_TYPE_STORAGE_BUFFER:
					return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				case DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER:
					return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				case DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT:
					return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				case DESCRIPTOR_SET_TYPE_SAMPLER:
					return VK_DESCRIPTOR_TYPE_SAMPLER;
				default:
					assert(false);
					break;
			}
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		}

		static U32 GetBindlessArraySize(DeviceVulkan& device, DescriptorSetType mask)
		{
			auto& feature_1_2 = device.features.features_1_2;
			auto& properties_1_2 = device.features.properties_1_2;
			switch (mask)
			{
			case DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE:
				return feature_1_2.descriptorBindingUniformTexelBufferUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindSampledImages / 4 : -1;
			case DESCRIPTOR_SET_TYPE_STORAGE_IMAGE:
				return feature_1_2.descriptorBindingStorageImageUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindStorageImages / 4 : -1;
			case DESCRIPTOR_SET_TYPE_STORAGE_BUFFER:
				return feature_1_2.descriptorBindingStorageBufferUpdateAfterBind ? properties_1_2.maxDescriptorSetUpdateAfterBindStorageBuffers / 4 : -1;
			case DESCRIPTOR_SET_TYPE_SAMPLER:
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
			for (U32 maskbit = 0; maskbit < DESCRIPTOR_SET_TYPE_COUNT; maskbit++)
			{
				if (maskFlags[maskbit] & 1)
				{
					DescriptorSetType mask = static_cast<DescriptorSetType>(maskbit);
					switch (mask)
					{
					case DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE:
						return feature_1_2.descriptorBindingUniformTexelBufferUpdateAfterBind;
					case DESCRIPTOR_SET_TYPE_STORAGE_IMAGE:
						return feature_1_2.descriptorBindingStorageImageUpdateAfterBind;
					case DESCRIPTOR_SET_TYPE_STORAGE_BUFFER:
						return feature_1_2.descriptorBindingStorageBufferUpdateAfterBind;
					case DESCRIPTOR_SET_TYPE_SAMPLER:
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
		imageInfo.sampler = VK_NULL_HANDLE;
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

	void BindlessDescriptorPool::SetBuffer(int binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.dstArrayElement = binding;
		write.descriptorCount = 1;
		write.dstSet = set;
		write.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device.device, 1, &write, 0, nullptr);
	}

	void BindlessDescriptorPool::SetUniformTexelBuffer(int binding, VkBufferView bufferView)
	{
		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		write.dstBinding = 0;
		write.dstArrayElement = binding;
		write.descriptorCount = 1;
		write.dstSet = set;
		write.pTexelBufferView = &bufferView;

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

		VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info =
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };

		uint32_t numDesc = numDescriptors;
		count_info.descriptorSetCount = 1;
		count_info.pDescriptorCounts = &numDesc;
		info.pNext = &count_info;

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
		layoutInfo(layout),
		setLayout(VK_NULL_HANDLE)
	{
		// Check bindless enable
		isBindless = layout.isBindless;
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
		else
		{
			for (U32 i = 0; i < device.GetNumThreads(); i++)
				perThreads.emplace_back(new PerThread());
		}

		std::vector<VkDescriptorSetLayoutBinding> bindings;
		for(U32 i = 0; i < VULKAN_NUM_BINDINGS; i++)
		{
			auto stages = stageForBinds[i];
			if (stages == 0)
				continue;
			
			// Create VkDescriptorSetLayoutBinding
			for (U32 maskbit = 0; maskbit < DESCRIPTOR_SET_TYPE_COUNT; maskbit++)
            {
				U32 types = 0;
				if (layout.masks[maskbit] & (1u << i))
				{
					auto& binding = layout.bindings[maskbit][i];

					// Calculate array size and pool size
					U32 arraySize = binding.arraySize;
					U32 poolArraySize = 0;
					if (arraySize == DescriptorSetLayout::UNSIZED_ARRAY || isBindless)
					{
						arraySize = GetBindlessArraySize(device, static_cast<DescriptorSetType>(maskbit));
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

					auto descriptorType = GetTypeBySetMask(static_cast<DescriptorSetType>(maskbit));
					bindings.push_back({
						binding.unrolledBinding,				// binding
						descriptorType, 						// descriptorType
						arraySize, 								// descriptorCount
						stages, 								// stageFlags
						nullptr 								// pImmutableSamplers
					});
					poolSize.push_back({ descriptorType, poolArraySize });
					descriptorCount = poolArraySize;
					types++;
				}
				ASSERT(types <= 1);
			}	
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
		if (!isBindless)
		{
			for (auto& perThread : perThreads)
				perThread->shouldBegin = true;
		}
	}

	void DescriptorSetAllocator::Clear()
	{
		for (auto& perThread : perThreads)
		{
			perThread->descriptorSetNodes.Clear();
			for (auto& pool : perThread->pools)
			{
				vkResetDescriptorPool(device.device, pool, 0);
				vkDestroyDescriptorPool(device.device, pool, nullptr);
			}
			perThread->pools.clear();
		}
	}

	std::pair<VkDescriptorSet, bool> DescriptorSetAllocator::GetOrAllocate(U32 threadIndex, HashValue hash)
	{
		ASSERT(threadIndex >= 0 && threadIndex < perThreads.size());
		PerThread& perThread = *perThreads[threadIndex];
		// free set map and push them into setVacants
		if (perThread.shouldBegin)
		{
			perThread.shouldBegin = false;
			perThread.descriptorSetNodes.BeginFrame();
		}

		DescriptorSetNode* node = perThread.descriptorSetNodes.Requset(hash);
		if (node != nullptr)
			return { node->set, true };
		
		node = perThread.descriptorSetNodes.RequestVacant(hash);
		if (node && node->set != VK_NULL_HANDLE)
			return { node->set, false };

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

		// create descriptor sets
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

		perThread.pools.push_back(pool);
		for(auto set : sets)
			perThread.descriptorSetNodes.MakeVacant(set);

		return { perThread.descriptorSetNodes.RequestVacant(hash)->set, false };
	}

	VkDescriptorPool DescriptorSetAllocator::AllocateBindlessPool(U32 numSets, U32 numDescriptors)
	{
		if (isBindless == false)
			return VK_NULL_HANDLE;

		VkDescriptorPoolSize size = poolSize[0];
		if (numDescriptors > size.descriptorCount)
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

	void BindlessDescriptorHandlerDeleter::operator()(BindlessDescriptorHandler* buffer)
	{
		buffer->device.bindlessDescriptorHandlers.free(buffer);
	}

	BindlessDescriptorHandler::~BindlessDescriptorHandler()
	{
		if (index >= 0)
			device.ReleaseBindlessResource(index, type);
	}

	BindlessDescriptorHandler::BindlessDescriptorHandler(DeviceVulkan& device_, BindlessReosurceType type_, I32 index_) :
		device(device_),
		type(type_),
		index(index_)
	{
	}

	void BindlessDescriptorHeap::Init(DeviceVulkan& device, DescriptorSetAllocator* setAllocator)
	{
		U32 descriptorCount = setAllocator->GetDescriptorCount();
		if (descriptorCount <= 0)
			return;

		VkDescriptorPool vkPool = setAllocator->AllocateBindlessPool(1, descriptorCount);
		if (vkPool == VK_NULL_HANDLE)
			return;

		pool = BindlessDescriptorPoolPtr(device.bindlessDescriptorPools.allocate(device, vkPool, setAllocator, 1, descriptorCount));
		if (!pool)
			return;

		pool->AllocateDescriptors(descriptorCount);

		freelist.reserve(descriptorCount);
		for (U32 i = 0; i < descriptorCount; ++i)
			freelist.push_back(descriptorCount - i - 1);

		initialized = true;
	}

	void BindlessDescriptorHeap::Destroy()
	{
		if (pool)
			pool.reset();

		initialized = false;
	}

	I32 BindlessDescriptorHeap::Allocate()
	{
		ASSERT(initialized);
		if (!freelist.empty())
		{
			I32 ret = freelist.back();
			freelist.pop_back();
			return ret;
		}
		return -1;
	}

	void BindlessDescriptorHeap::Free(I32 index)
	{
		ASSERT(initialized);
		if (index >= 0)
			freelist.push_back(index);
	}

	VkDescriptorSet BindlessDescriptorHeap::GetDescriptorSet() const
	{
		ASSERT(initialized);
		return pool->GetDescriptorSet();
	}

	BindlessDescriptorPool& BindlessDescriptorHeap::GetPool()
	{
		ASSERT(initialized);
		return *pool;
	}
}
}