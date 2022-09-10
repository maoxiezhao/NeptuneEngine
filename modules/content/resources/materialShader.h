#pragma once

#include "renderer\enums.h"
#include "shader.h"
#include "texture.h"
#include "materialParams.h"

namespace VulkanTest
{
	enum class MaterialDomain
	{
		Object = 0,
		Count
	};

	enum class MaterialType
	{
		Standard,
		Shader,
		Visual
	};

	struct MaterialInfo
	{
		MaterialDomain domain = MaterialDomain::Object;
		BlendMode blendMode = BlendMode::BLENDMODE_OPAQUE;
		ObjectDoubleSided side = ObjectDoubleSided::OBJECT_DOUBLESIDED_FRONTSIDE;
		MaterialType type;
		char shaderPath[MAX_PATH_LENGTH];
	};

	class VULKAN_TEST_API MaterialShader
	{
	public:
		virtual ~MaterialShader();

		static MaterialShader* Create(const String& name, InputMemoryStream& mem, const MaterialInfo& info, ResourceManager& resManager);

		bool IsReady()const;
		const Shader* GetShader()const;

		struct BindParameters
		{
			GPU::CommandList* cmd;
			RENDERPASS renderPass;
		};
		virtual void Bind(BindParameters& params) = 0;
		virtual void Unload();

	protected:
		MaterialShader(const String& name_);
		bool Load(InputMemoryStream& mem, const MaterialInfo& info_, ResourceManager& resManager);

		virtual bool OnLoaded() = 0;

		bool isLoaded = false;
		String name;
		ResPtr<Shader> shader;
		MaterialInfo info;
	};
}