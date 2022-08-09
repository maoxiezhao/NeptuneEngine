#include "material.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Material);

	namespace LuaAPI
	{
		int color(lua_State* l)
		{
			const F32x4 c = LuaUtils::Get<F32x4>(l, 1);
			lua_getglobal(l, "this");
			MaterialParams* params = (MaterialParams*)lua_touserdata(l, -1);
			lua_pop(l, 1);
			params->color = c;
			return 0;
		}

		int texture(lua_State* l)
		{
			return 0;
		}
	}

	bool MaterialParams::Load(U64 size, const U8* mem, MaterialFactory& factory)
	{
		if (size <= 0)
			return false;

		LuaConfig& luaConfig = factory.GetLuaConfig();
		luaConfig.AddLightUserdata("this", this);

		const Span<const char> content((const char*)mem, (U32)size);
		return luaConfig.Load(content);
	}

	void MaterialParams::Unload()
	{
		for (auto texture : textures)
			texture.reset();
	}

	MaterialFactory::MaterialFactory()
	{	
#define DEFINE_LUA_FUNC(func) luaConfig.AddFunc(#func, LuaAPI::func);
		DEFINE_LUA_FUNC(color)
		DEFINE_LUA_FUNC(texture)

#undef DEFINE_LUA_FUNC
	}

	MaterialFactory::~MaterialFactory()
	{
	}

	Resource* MaterialFactory::CreateResource(const Path& path)
	{
		return CJING_NEW(Material)(path, *this);
	}

	void MaterialFactory::DestroyResource(Resource* res)
	{
		CJING_SAFE_DELETE(res);
	}

	Material::Material(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_)
	{
	}

	Material::~Material()
	{
	}

	void Material::Bind(MaterialShader::BindParameters& params)
	{
		ASSERT(IsReady());
		materialShader->Bind(params);
	}

	bool Material::IsReady() const
	{
		return (Resource::IsReady() && materialShader && materialShader->IsReady());
	}

	const Shader* Material::GetShader() const
	{
		return materialShader ? materialShader->GetShader() : nullptr;
	}

	bool Material::OnLoaded()
	{
		PROFILE_FUNCTION();
		ASSERT(materialShader == nullptr);

		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;

		// Check material header
		MaterialHeader header;
		InputMemoryStream inputMem(dataChunk->Data(), dataChunk->Size());
		inputMem.Read<MaterialHeader>(header);
		if (header.magic != MaterialHeader::MAGIC)
		{
			Logger::Warning("Unsupported material file %s", GetPath());
			return false;
		}

		if (header.version != MaterialHeader::LAST_VERSION)
		{
			Logger::Warning("Unsupported version of material %s", GetPath());
			return false;
		}

		auto& factory = static_cast<MaterialFactory&>(GetResourceFactory());

		// Create material shader
		const String name(GetPath().c_str());
		materialShader = MaterialShader::Create(name, inputMem, header.materialInfo, factory);
		if (materialShader == nullptr)
		{
			Logger::Warning("Failed to create material shader %s", GetPath().c_str());
			return false;
		}

		// Load material params
		if (!params.Load(dataChunk->Size() - inputMem.GetPos(), dataChunk->Data() + inputMem.GetPos(), factory))
		{
			Logger::Warning("Failed to load material params %s", GetPath().c_str());
			return false;
		}

		return true;
	}

	void Material::OnUnLoaded()
	{
		if (materialShader != nullptr)
		{
			materialShader->Unload();
			CJING_SAFE_DELETE(materialShader);
			materialShader = nullptr;
		}

		params.Unload();
	}
}