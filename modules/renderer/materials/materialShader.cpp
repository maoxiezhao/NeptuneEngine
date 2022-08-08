#include "materialShader.h"
#include "material.h"
#include "renderer\renderer.h"

#include "objectMaterialShader.h"

namespace VulkanTest
{
	MaterialShader::~MaterialShader()
	{
	}

	MaterialShader* MaterialShader::Create(const String& name, InputMemoryStream& mem, const MaterialInfo& info, MaterialFactory& factory)
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

		if (!ret->Load(mem, info, factory))
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

	bool MaterialShader::Load(InputMemoryStream& mem, const MaterialInfo& info_, MaterialFactory& factory)
	{
		ASSERT(isLoaded == false);
		info = info_;

		if (info_.useCustomShader && info_.shaderSize > 0)
		{
			// Load shader from memory stream
			if (mem.GetPos() + info.shaderSize > mem.Size())
				return false;

			shader = ResPtr<Shader>(CJING_NEW(Shader)(Path(name.c_str()), factory));
			shader->SetOwnedBySelf(true);
			if (!shader->Create(info.shaderSize, (const U8*)mem.GetBuffer() + mem.GetPos()))
				return false;
		}
		else if (info_.shaderPath[0] != 0)
		{
			// Load shader from target path
			shader = factory.GetResourceManager().LoadResourcePtr<Shader>(Path(info.shaderPath));
			if (!shader)
				return false;
		}

		if (!OnLoaded())
			return false;

		isLoaded = true;
		return true;
	}
}