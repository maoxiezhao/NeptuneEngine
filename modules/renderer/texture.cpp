#include "texture.h"
#include "renderer.h"
#include "gpu\vulkan\TextureFormatLayout.h"
#include "stb\stb_image.h"

namespace VulkanTest
{
	DEFINE_RESOURCE(Texture);

	Texture::Texture(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_)
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

		if (data != nullptr)
		{
			GPU::SubresourceData resData = {};
			resData.data = data;
			handle = device->CreateImage(info, &resData);
		}
		else
		{
			handle = device->CreateImage(info, nullptr);
		}

		bool isReady = bool(handle);
		OnCreated(isReady ? Resource::State::READY : Resource::State::FAILURE);
		return isReady;
	}

	void Texture::Destroy()
	{
		DeleteObjectNow();
	}

	I32 Texture::GetDescriptorIndex()
	{
		if (!IsReady())
			return -1;

		GPU::DeviceVulkan* device = Renderer::GetDevice();
		if (!bindless)
			bindless = device->CreateBindlessSampledImage(GetImage()->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		
		return bindless->GetIndex();
	}

	bool Texture::Init(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
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
		return true;
	}

	bool Texture::Load()
	{
		PROFILE_FUNCTION();
		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;

		const U8* imgData = (const U8*)dataChunk->Data();
		switch (header.type)
		{
		case TextureResourceType::TGA:
			return LoadTextureTGA(header, imgData, dataChunk->Size());
		case TextureResourceType::INTERNAL:
			return LoadTextureInternal(header, imgData, dataChunk->Size());
		default:
			ASSERT(false);
			return false;
		}
	}

	void Texture::Unload()
	{
		handle.reset();
		bindless.reset();
	}

#pragma pack(1)
	struct TGAHeader
	{
		U8 idLength;
		U8 colourMapType;
		U8 dataType;
		U16 colourMapOrigin;
		U16 colourMapLength;
		U8 colourMapDepth;
		U16 xOrigin;
		U16 yOrigin;
		U16 width;
		U16 height;
		U8 bitsPerPixel;
		U8 imageDescriptor;
	};
#pragma pack()

	static void FlipVertical(U32* image, int width, int height)
	{
		PROFILE_FUNCTION();
		for (int j = 0; j < height / 2; ++j)
		{
			int row_offset = width * j;
			int inv_j = height - j - 1;
			int inv_row_offset = width * inv_j;
			for (int i = 0; i < width; ++i)
			{
				U32 tmp = image[i + row_offset];
				image[i + row_offset] = image[i + inv_row_offset];
				image[i + inv_row_offset] = tmp;
			}
		}
	}

	bool Texture::LoadTextureTGA(const TextureHeader& header, const U8* imgData, U64 size)
	{
		PROFILE_FUNCTION();
		InputMemoryStream inputMem(imgData, size);

		TGAHeader tgaHeader;
		inputMem.Read<TGAHeader>(tgaHeader);
		int image_size = tgaHeader.width * tgaHeader.height * 4;
		if (tgaHeader.dataType != 10 && tgaHeader.dataType != 2)
		{
			int w, h, cmp;
			stbi_uc* stbData = stbi_load_from_memory(static_cast<const stbi_uc*>(imgData) + 7, size - 7, &w, &h, &cmp, 4);
			if (!stbData)
			{
				Logger::Warning("Unsupported texture format %s", GetPath().c_str());
				return false;
			}
			
			GPU::ImageCreateInfo info = GPU::ImageCreateInfo::ImmutableImage2D(w, h, VK_FORMAT_R8G8B8A8_UNORM);
			GPU::SubresourceData resData;
			resData.data = stbData;

			GPU::DeviceVulkan* device = Renderer::GetDevice();
			handle = device->CreateImage(info, &resData);
			if (handle)
				this->info = info;

			bool ret = bool(handle);
			stbi_image_free(stbData);
			return ret;
		}
		else
		{
			int imageSize = tgaHeader.width * tgaHeader.height * 4;
			OutputMemoryStream data;
			data.Resize(imageSize);

			int pixelCount = tgaHeader.width * tgaHeader.height;
			U32 bytesPerPixel = tgaHeader.bitsPerPixel / 8;

			if (tgaHeader.dataType == 10)
			{
				U8* out = data.Data();
				U8 byte;
				struct Pixel {
					U8 uint8[4];
				} pixel;
				do
				{
					inputMem.Read(&byte, sizeof(byte));
					if (byte < 128)
					{
						U8 count = byte + 1;
						for (U8 i = 0; i < count; ++i)
						{
							inputMem.Read(&pixel, bytesPerPixel);
							out[0] = pixel.uint8[2];
							out[1] = pixel.uint8[1];
							out[2] = pixel.uint8[0];
							if (bytesPerPixel == 4)
								out[3] = pixel.uint8[3];
							else
								out[3] = 255;
							out += 4;
						}
					}
					else
					{
						byte -= 127;
						inputMem.Read(&pixel, bytesPerPixel);
						for (int i = 0; i < byte; ++i)
						{
							out[0] = pixel.uint8[2];
							out[1] = pixel.uint8[1];
							out[2] = pixel.uint8[0];
							if (bytesPerPixel == 4)
								out[3] = pixel.uint8[3];
							else
								out[3] = 255;
							out += 4;
						}
					}
				} while (out - data.Data() < pixelCount * 4);
			}
			else
			{
				U8* image_dest = data.Data();

				PROFILE_BLOCK("read");
				if (bytesPerPixel == 4)
				{
					PROFILE_BLOCK("read 4BPP");
					inputMem.Read(image_dest, header.width * header.height * bytesPerPixel);
					for (U32 y = 0; y < header.height; y++)
					{
						U32 idx = y * header.width * bytesPerPixel;
						U8* __restrict cursor = &image_dest[idx];
						const U8* row_end = cursor + header.width * bytesPerPixel;
						while (cursor != row_end)
						{
							const U8 tmp = cursor[0];
							cursor[0] = cursor[2];
							cursor[2] = tmp;
							cursor += 4;
						}
					}
				}
				else
				{
					PROFILE_BLOCK("read 3BPP");
					for (U32 y = 0; y < header.height; y++)
					{
						U32 idx = y * header.width * 4;
						for (U32 x = 0; x < header.width; x++)
						{
							inputMem.Read(&image_dest[idx + 2], sizeof(U8));
							inputMem.Read(&image_dest[idx + 1], sizeof(U8));
							inputMem.Read(&image_dest[idx + 0], sizeof(U8));
							image_dest[idx + 3] = 255;
							idx += 4;
						}
					}
				}
			}

			// Need to flip
			if ((tgaHeader.imageDescriptor & 32) == 0) 
				FlipVertical((U32*)data.Data(), tgaHeader.width, tgaHeader.height);

			// Create gpu image
			GPU::ImageCreateInfo info = GPU::ImageCreateInfo::ImmutableImage2D(tgaHeader.width, tgaHeader.height, VK_FORMAT_R8G8B8A8_UNORM);
			GPU::SubresourceData resData;
			resData.data = data.Data();
			GPU::DeviceVulkan* device = Renderer::GetDevice();
			handle = device->CreateImage(info, &resData);
			if (handle)
				this->info = info;

			return bool(handle);
		}
	}

	bool Texture::LoadTextureInternal(const TextureHeader& header, const U8* imgData, U64 size)
	{
		PROFILE_FUNCTION();
		GPU::ImageCreateInfo info = GPU::ImageCreateInfo::ImmutableImage2D(header.width, header.height, header.format);
		info.levels = header.mips;

		GPU::TextureFormatLayout layout;
		layout.SetTexture2D(header.format, header.width, header.height, 1, header.mips);
		layout.SetBuffer((void*)imgData, size);

		GPU::SubresourceData resData[16];
		for (U32 i = 0; i < header.mips; i++)
			resData[i].data = layout.Data(0, i);

		GPU::DeviceVulkan* device = Renderer::GetDevice();
		handle = device->CreateImage(info, resData);
		if (handle)
			this->info = info;

		return bool(handle);
	}

	void Texture::UpdateTexture(const InputMemoryStream& data)
	{
		PROFILE_FUNCTION();

		if (!handle)
			return;

		auto device = Renderer::GetDevice();
		ASSERT(device != nullptr);
		auto cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
		cmd->ImageBarrier(*handle, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

		U8* mem = static_cast<U8*>(cmd->UpdateImage(*handle));
		memcpy(mem, data.GetBuffer(), data.Size());

		cmd->ImageBarrier(*handle,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

		device->Submit(cmd);
	}
}