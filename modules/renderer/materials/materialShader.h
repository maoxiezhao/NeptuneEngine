#pragma once

#include "renderer\enums.h"
#include "renderer\shader.h"
#include "renderer\texture.h"

namespace VulkanTest
{
	struct MaterialFactory;

	enum class MaterialDomain
	{
		Object = 0,
		Count
	};

	struct MaterialInfo
	{
		MaterialDomain domain = MaterialDomain::Object;
		BlendMode blendMode = BlendMode::BLENDMODE_OPAQUE;
		ObjectDoubleSided side = ObjectDoubleSided::OBJECT_DOUBLESIDED_FRONTSIDE;

		bool useCustomShader = false;
		U64 shaderSize = 0;
		char shaderPath[MAX_PATH_LENGTH];
	};

	class VULKAN_TEST_API MaterialShader
	{
	public:
		virtual ~MaterialShader();

		static MaterialShader* Create(const String& name, InputMemoryStream& mem, const MaterialInfo& info, MaterialFactory& factory);

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
		bool Load(InputMemoryStream& mem, const MaterialInfo& info_, MaterialFactory& factory);

		virtual bool OnLoaded() = 0;

		bool isLoaded = false;
		String name;
		ResPtr<Shader> shader;
		MaterialInfo info;
	};
}