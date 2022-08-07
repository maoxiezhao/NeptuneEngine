#pragma once

#include "core\resource\resource.h"
#include "gpu\vulkan\device.h"
#include "math\color.h"

namespace VulkanTest
{
#pragma pack(1)
	struct TextureHeader
	{
		static constexpr U32 LAST_VERSION = 0;
		static constexpr U32 MAGIC = '_CST';
		U32 magic = MAGIC;
		U32 version = LAST_VERSION;
		VkFormat format;
		U32 width;
		U32 height;
		U32 mips;
	};
#pragma pack()

	class VULKAN_TEST_API Texture final : public Resource
	{
	public:
		DECLARE_RESOURCE(Texture);

		enum TextureType
		{
			DIFFUSE,
			NORMAL,
			SPECULAR,
			SHININESS,
			AMBIENT,
			EMISSIVE,
			REFLECTION,
			COUNT
		};

		Texture(const Path& path_, ResourceFactory& resFactory_);
		virtual ~Texture();

		bool Create(U32 w, U32 h, VkFormat format, const void* data);
		void Destroy();

		GPU::Image* GetImage() {
			return handle ? handle.get() : nullptr;
		}
		const GPU::ImageCreateInfo& GetImageInfo()const {
			return info;
		}

	protected:
		bool OnLoaded(U64 size, const U8* mem) override;
		void OnUnLoaded() override;

	private:
		GPU::ImagePtr handle;
		GPU::ImageCreateInfo info;
	};
}