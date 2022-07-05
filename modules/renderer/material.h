#pragma once

#include "core\resource\resource.h"
#include "core\resource\resourceManager.h"
#include "core\scripts\luaConfig.h"
#include "math\color.h"
#include "renderer\shader.h"

namespace VulkanTest
{
	struct MaterialFactory : public ResourceFactory
	{
	public:
		MaterialFactory();
		virtual ~MaterialFactory();

		LuaConfig& GetLuaConfig() {
			return luaConfig;
		}

	protected:
		Resource* CreateResource(const Path& path) override;
		void DestroyResource(Resource* res) override;

	private:
		LuaConfig luaConfig;
	};

	class VULKAN_TEST_API Material final : public Resource
	{
	public:
		DECLARE_RESOURCE(Material);

		Material(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Material();
	
		Color4 GetColor() const { return color; }
		void SetColor(const F32x4& color_) { color = Color4(color_); }

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		Color4 color;
		ResPtr<Shader> shader;
	};
}