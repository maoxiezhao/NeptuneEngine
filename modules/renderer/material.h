#pragma once

#include "core\resource\resource.h"
#include "core\resource\resourceManager.h"
#include "core\scripts\luaConfig.h"
#include "math\color.h"
#include "renderer\shader.h"
#include "enums.h"

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

		enum FLAGS
		{
			EMPTY = 0,
			DIRTY = 1 << 0,
			CAST_SHADOW = 1 << 1,
			DOUBLE_SIDED = 1 << 2,
		};

		Material(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Material();
	
		Color4 GetColor() const { return color; }
		void SetColor(const F32x4& color_) { color = Color4(color_); }
		BlendMode GetBlendMode()const { return blendMode; }
		bool IsCastingShadow() const { return flags & CAST_SHADOW; }
		bool IsDoubleSided() const { return  flags & DOUBLE_SIDED; }

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		Color4 color;
		BlendMode blendMode = BlendMode::BLENDMODE_OPAQUE;
		U32 flags = EMPTY;
		ResPtr<Shader> shader;
	};
}