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
			Material* material = (Material*)lua_touserdata(l, -1);
			lua_pop(l, 1);
			material->SetColor(c);
			return 0;
		}

		int texture(lua_State* l)
		{
			return 0;
		}
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
		Resource(path_, resFactory_)
	{
	}

	Material::~Material()
	{
	}

	bool Material::OnLoaded(U64 size, const U8* mem)
	{
		PROFILE_FUNCTION();

		MaterialFactory& factory = static_cast<MaterialFactory&>(GetResourceFactoyr());
		LuaConfig& luaConfig = factory.GetLuaConfig();
		luaConfig.AddLightUserdata("this", this);

		const Span<const char> content((const char*)mem, (U32)size);
		if (!luaConfig.Load(content))
			return false;

		return true;
	}

	void Material::OnUnLoaded()
	{
	}
}