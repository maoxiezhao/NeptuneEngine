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
		resData.rowLength = w * formatInfo.GetFormatStride() / formatInfo.GetFormatBlockSize();
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
		// TODO
		ASSERT(false);
		return false;
	}

	void Texture::OnUnLoaded()
	{
		handle.reset();
	}
}