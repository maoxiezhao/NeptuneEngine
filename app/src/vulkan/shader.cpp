#include "shader.h"
#include "device.h"

ShaderProgram::ShaderProgram(DeviceVulkan* device, const ShaderProgramInfo& info) :
	mDevice(device)
{
	if (info.VS != nullptr)
		SetShader(ShaderStage::VS, info.VS);
	if (info.PS != nullptr)
		SetShader(ShaderStage::PS, info.PS);

	device->BakeShaderProgram(*this);
}

ShaderProgram::~ShaderProgram()
{
}

void ShaderProgram::SetShader(ShaderStage stage, Shader* shader)
{
	mShaders[(int)stage] = shader;
}
