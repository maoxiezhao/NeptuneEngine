## Shader resource pipeline
***************************************
简述下整个Shader resource pipeline流程  

### 1. Structure
```
// DescriptorSetLayout描述  
// DescriptorSetAllocator根据该结构体创建对应的VkDescriptorSetLayout  
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
    // 基于各个set类型对应的bindingMasks
    U32 masks[static_cast<U32>(SetMask::COUNT)] = {};
    uint8_t arraySize[VULKAN_NUM_BINDINGS] = {};
};

// Shader Compiling阶段反射获取
struct ShaderResourceLayout
{
		DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
		uint32_t pushConstantSize = 0;
};

// Combined resource layout
// 在bakeProgram阶段创建DescriptorSetAllocator
struct CombinedResourceLayout
{
    U32 descriptorSetMask = 0;
    U32 stagesForSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    U32 stagesForBindings[VULKAN_NUM_DESCRIPTOR_SETS][VULKAN_NUM_BINDINGS] = {};
    DescriptorSetLayout sets[VULKAN_NUM_DESCRIPTOR_SETS];
    VkPushConstantRange pushConstantRange = {};
    HashValue pushConstantHash = 0;
};
```
