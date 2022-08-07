#include "texture.h"
#include "renderer.h"
#include "gpu\vulkan\TextureFormatLayout.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Texture);

	Texture::Texture(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Texture::~Texture()
	{
		ASSERT(IsEmpty());
	}

	bool Texture::Create(U32 w, U32 h, VkFormat format, const void* data)
	{
		GPU::DeviceVulkan* device = Renderer::GetDevice();
		info = GPU::ImageCreateInfo::ImmutableImage2D(w, h, format);

		auto formatInfo = GPU::GetFormatInfo(format);
		GPU::SubresourceData resData = {};
		resData.data = data;
		handle = device->CreateImage(info, &resData);

		bool isReady = bool(handle);
		OnCreated(isReady ? Resource::State::READY : Resource::State::FAILURE);
		return isReady;
	}

	void Texture::Destroy()
	{
		DoUnload();
	}

	bool Texture::OnLoaded(U64 size, const U8* mem)
	{
		PROFILE_FUNCTION();
		InputMemoryStream inputMem(mem, size);

		TextureHeader header;
		inputMem.Read<TextureHeader>(header);
		if (header.magic != TextureHeader::MAGIC)
		{
			Logger::Warning("Unsupported texture file %s", GetPath());
			return false;
		}

		if (header.version != TextureHeader::LAST_VERSION)
		{
			Logger::Warning("Unsupported version of texture %s", GetPath());
			return false;
		}

		const U8* imgData = mem + sizeof(TextureHeader);

		GPU::ImageCreateInfo info = GPU::ImageCreateInfo::ImmutableImage2D(header.width, header.height, header.format);
		info.levels = header.mips;

		GPU::TextureFormatLayout layout;
		layout.SetTexture2D(header.format, header.width, header.height, 1, header.mips);
		layout.SetBuffer((void*)imgData, size - sizeof(TextureHeader));

		GPU::SubresourceData resData[16];
		for (int i = 0; i < header.mips; i++)
			resData[i].data = layout.Data(0, i);
		
		GPU::DeviceVulkan* device = Renderer::GetDevice();
		handle = device->CreateImage(info, resData);
		if (handle)
			this->info = info;

		return bool(handle);
	}

	void Texture::OnUnLoaded()
	{
		handle.reset();
	}
}