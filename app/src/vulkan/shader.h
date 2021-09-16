#pragma once

#include "definition.h"

struct PipelineLayout
{
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
};

struct Shader
{
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

private:
    void SetShader(ShaderStage stage, Shader* shader);

    DeviceVulkan* mDevice;
    PipelineLayout* mPipelineLayout = nullptr;
    Shader* mShaders[UINT(ShaderStage::Count)] = {};
    uint32_t mShaderCount = 0;
};
