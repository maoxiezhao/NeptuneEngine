#pragma once

#include "definition.h"

class PipelineLayout : public HashedObject<PipelineLayout>
{
public:
    PipelineLayout(DeviceVulkan& device);
    ~PipelineLayout();

    VkPipelineLayout GetLayout()const
    {
        return mPipelineLayout;
    }

private:
    DeviceVulkan& mDevice;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
};

class Shader : public HashedObject<Shader>
{
public:
    Shader(DeviceVulkan& device, ShaderStage shaderStage, VkShaderModule shaderModule);
    Shader(DeviceVulkan& device, ShaderStage shaderStage, const void* pShaderBytecode, size_t bytecodeLength);
    ~Shader();

    VkShaderModule GetModule()const
    {
        return mShaderModule;
    }

private:
    DeviceVulkan& mDevice;
	ShaderStage mShaderStage;
	VkShaderModule mShaderModule = VK_NULL_HANDLE;
};

struct ShaderProgramInfo
{
    Shader* VS = nullptr;
    Shader* PS = nullptr;
};

struct ShaderProgram
{
public:
    ShaderProgram(DeviceVulkan* device, const ShaderProgramInfo& info);
    ~ShaderProgram();

    PipelineLayout* GetPipelineLayout()const
    {
        return mPipelineLayout;
    }

    inline const Shader* GetShader(ShaderStage stage) const
    {
        return mShaders[UINT(stage)];
    }

    bool IsEmpty()const
    {
        return mShaderCount > 0;
    }

    void AddPipeline(HashValue hash, VkPipeline pipeline);
    VkPipeline GetPipeline(HashValue hash);

private:
    void SetShader(ShaderStage stage, Shader* shader);

    DeviceVulkan* mDevice;
    PipelineLayout* mPipelineLayout = nullptr;
    Shader* mShaders[UINT(ShaderStage::Count)] = {};
    uint32_t mShaderCount = 0;
    std::unordered_map<HashValue, VkPipeline> mPipelines;
};
