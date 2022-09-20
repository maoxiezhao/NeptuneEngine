#include "materialShader.h"
#include "material.h"
#include "renderer\renderer.h"

#include "objectMaterialShader.h"

namespace VulkanTest
{
	MaterialShader::~MaterialShader()
	{
	}

	MaterialShader* MaterialShader::Create(const String& name, InputMemoryStream& mem, const MaterialInfo& info)
	{
		MaterialShader* ret = nullptr;
		switch (info.domain)
		{
		case MaterialDomain::Object:
			ret = CJING_NEW(ObjectMaterialShader)(name);
			break;
		default:
			ASSERT(false);
			break;
		}

		if (!ret->Load(mem, info))
		{
			CJING_DELETE(ret);
			return nullptr;
		}
		return ret;
	}

	bool MaterialShader::IsReady() const
	{
		return isLoaded;
	}

	const Shader* MaterialShader::GetShader() const
	{
		return shader.get();
	}

	void MaterialShader::Unload()
	{
		isLoaded = false;
		shader.reset();
	}

	MaterialShader::MaterialShader(const String& name_) :
		name(name_)
	{
	}

	bool MaterialShader::Load(InputMemoryStream& mem, const MaterialInfo& info_)
	{
		ASSERT(isLoaded == false);
		auto factory = ResourceManager::GetFactory(Shader::ResType);
		ASSERT(factory);

		info = info_;

		if (info_.type == MaterialType::Visual)
		{
			// Load generated shader from the chunk memory
			ASSERT(false);
		}
		else if (info_.type == MaterialType::Shader)
		{
			// Load shader from target path
			if (info_.shaderPath[0] == 0)
				return false;

			shader = ResourceManager::LoadResource<Shader>(Path(info.shaderPath));
			if (!shader)
				return false;
		}

		if (!OnLoaded())
			return false;

		isLoaded = true;
		return true;
	}
}