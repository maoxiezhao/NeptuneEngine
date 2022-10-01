#pragma once

#include "content\binaryResource.h"
#include "gpu\vulkan\device.h"
#include "math\color.h"

namespace VulkanTest
{
	enum TextureResourceType
	{
		TGA,		// Only used for editor
		INTERNAL
	};

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
		TextureResourceType type = TextureResourceType::INTERNAL;
	};
#pragma pack()

	class VULKAN_TEST_API Texture : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(Texture);

		enum TextureType
		{
			DIFFUSE,
			NORMAL,
			SURFACEMAP,
			SPECULAR,
			SHININESS,
			AMBIENT,
			EMISSIVE,
			REFLECTION,
			CUSTOM,
			COUNT
		};

		Texture(const ResourceInfo& info);
		virtual ~Texture();

		bool Create(U32 w, U32 h, VkFormat format, const void* data);
		void Destroy();
		I32  GetDescriptorIndex();

		GPU::Image* GetImage() {
			return handle ? handle.get() : nullptr;
		}
		const GPU::ImageCreateInfo& GetImageInfo()const {
			return info;
		}

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

	protected:
		bool LoadTextureTGA(const TextureHeader& header, const U8* imgData, U64 size);
		bool LoadTextureInternal(const TextureHeader& header, const U8* imgData, U64 size);
		void UpdateTexture(const InputMemoryStream& data);

		TextureHeader header;
		GPU::ImagePtr handle;
		GPU::ImageCreateInfo info;
		GPU::BindlessDescriptorPtr bindless;
	};
}