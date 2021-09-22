#pragma once

#include "definition.h"

class PipelineLayout : public HashedObject<PipelineLayout>
{
public:
    PipelineLayout(DeviceVulkan& device_);
    ~PipelineLayout();

    VkPipelineLayout GetLayout()const
    {
        return pipelineLayout;
    }

private:
    DeviceVulkan& device;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};

class Shader : public HashedObject<Shader>
{
public:
    Shader(DeviceVulkan& device_, ShaderStage shaderStage_, VkShaderModule shaderModule_);
    Shader(DeviceVulkan& device_, ShaderStage shaderStage_, const void* pShaderBytecode, size_t bytecodeLength);
    ~Shader();

    VkShaderModule GetModule()const
    {
        return shaderModule;
    }

private:
    DeviceVulkan& device;
	ShaderStage shaderStage;
	VkShaderModule shaderModule = VK_NULL_HANDLE;
};

struct ShaderProgramInfo
{
    Shader* vs = nullptr;
    Shader* ps = nullptr;
};

struct ShaderProgram
{
public:
    ShaderProgram(DeviceVulkan* device_, const ShaderProgramInfo& info);
    ~ShaderProgram();

    PipelineLayout* GetPipelineLayout()const
    {
        return pipelineLayout;
    }

    inline const Shader* GetShader(ShaderStage stage) const
    {
        return shaders[UINT(stage)];
    }

    bool IsEmpty()const
    {
        return shaderCount > 0;
    }

    void AddPipeline(HashValue hash, VkPipeline pipeline);
    VkPipeline GetPipeline(HashValue hash);

private:
    void SetShader(ShaderStage stage, Shader* shader);

    DeviceVulkan* device;
    PipelineLayout* pipelineLayout = nullptr;
    Shader* shaders[UINT(ShaderStage::Count)] = {};
    uint32_t shaderCount = 0;
    std::unordered_map<HashValue, VkPipeline> pipelines;
};
