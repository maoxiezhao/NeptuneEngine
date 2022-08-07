#include "textureImporter.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"
#include "core\filesystem\filesystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb\stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb\stb_image_resize.h"

namespace VulkanTest
{
namespace Editor
{
    namespace TextureCompressor
    {
        struct Input
        {
            struct Image 
            {
                OutputMemoryStream pixels;
                U32 mip;
                U32 face;
                U32 slice;
            };

            Array<Image> images;
            U32 w;
            U32 h;
            U32 slices;
            U32 mips;
            VkFormat format;
            bool hasAlpha = false;
            bool isCubemap = false;

            void Init(U32 w_, U32 h_, VkFormat format_, U32 slices_, U32 mips_)
            {
                w = w_;
                h = h_;
                slices = slices_;
                mips = mips_;
                format = format_;
            }

            void Add(Span<const U8> data, U32 face, U32 slice, U32 mip)
            {
                auto& img = images.emplace();
                img.face = face;
                img.mip = mip;
                img.slice = slice;
                img.pixels.Reserve(data.length());
                img.pixels.Write(data.begin(), data.length());
            }

            const Image& Get(U32 face, U32 slice, U32 mip) const 
            {
                for (const Image& img : images) {
                    if (img.face == face && img.mip == mip && img.slice == slice)
                        return img;
                }
                ASSERT(false);
                return images[0];
            }
        };

        struct Options
        {
            bool compress = true;
            bool generateMipmaps = false;
        };

        void CompressRGBA(Span<const U8> input, OutputMemoryStream& output, U32 width, U32 height)
        {
            PROFILE_FUNCTION();
            output.Write(input.begin(), input.length());
        }

        void CompressBC3(Span<const U8> input, OutputMemoryStream& output, U32 width, U32 height)
        {
        }

        void CompressBC1(Span<const U8> input, OutputMemoryStream& output, U32 width, U32 height)
        {
        }

        U32 MipLevelsCount(U32 width, U32 height, bool useMipLevels)
        {
            return useMipLevels ? 1 + (U32)std::log2(std::max(width, height)) : 1;
        }

        void ComputeMip(VkFormat format, Span<const U8> src, Span<U8> dst, U32 w, U32 h, U32 dstW, U32 dstH)
        {
            PROFILE_FUNCTION();
            switch (format)
            {
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_R8G8B8_SNORM:
            case VK_FORMAT_R8G8B8_USCALED:
            case VK_FORMAT_R8G8B8_SSCALED:
            case VK_FORMAT_R8G8B8_UINT:
            case VK_FORMAT_R8G8B8_SINT:
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R8G8B8A8_USCALED:
            case VK_FORMAT_R8G8B8A8_SSCALED:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SINT:
                if (!stbir_resize_uint8(src.begin(), w, h, 0, dst.begin(), dstW, dstH, 0, 4))
                {
                    Logger::Warning("Cannot resize image");
                    return;
                }
                break;
        
            case VK_FORMAT_R8G8B8A8_SRGB:
                if (!stbir_resize_uint8_srgb(src.begin(), w, h, 0, dst.begin(), dstW, dstH, 0, 4, 3, STBIR_ALPHA_CHANNEL_NONE))
                {
                    Logger::Warning("Cannot resize image");
                    return;
                }
                break;

            case VK_FORMAT_R32_SFLOAT:
            case VK_FORMAT_R32G32_SFLOAT:
            case VK_FORMAT_R32G32B32_SFLOAT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                ASSERT(false);
                break;

            default:
                ASSERT(false);
                break;
            }
        }

        bool WriteTexture(void (*compressor)(Span<const U8>, OutputMemoryStream&, U32, U32), const Input& input, const Options& options, OutputMemoryStream& dst)
        {
            U32 sourceMipLevels = input.mips;
            bool useMipLevels = (options.generateMipmaps || sourceMipLevels > 1) && (input.w > 1 || input.h > 1);
            U32 mipLevels = MipLevelsCount(input.w, input.h, useMipLevels);

            GPU::TextureFormatLayout layout;
            layout.SetTexture2D(input.format, input.w, input.h, mipLevels);
            dst.Reserve(dst.Size() + layout.GetRequiredSize());

            Array<U8> curMipData;
            Array<U8> prevMipData;
            const U32 faces = input.isCubemap ? 6 : 1;
            for (U32 slice = 0; slice < input.slices; slice++)
            {
                for (U32 face = 0; face < faces; ++face)
                {
                    for (U32 mip = 0; mip < mipLevels; ++mip)
                    {
                        U32 mipW = std::max(input.w >> mip, 1u);
                        U32 mipH = std::max(input.h >> mip, 1u);
                        if (mip == 0)
                        {
                            const Input::Image& src = input.Get(face, slice, mip);
                            compressor(src.pixels, dst, mipW, mipH);
                        }
                        else
                        {
                            if (options.generateMipmaps)
                            {
                                curMipData.resize(mipW * mipH * layout.GetBlockStride());

                                U32 prevW = std::max(input.w >> (mip - 1), 1u);
                                U32 prevH = std::max(input.h >> (mip - 1), 1u);
                                ComputeMip(
                                    input.format,
                                    mip == 1 ? Span<const U8>(input.Get(face, slice, 0).pixels) : prevMipData,
                                    curMipData,
                                    prevW, prevH, 
                                    mipW, mipH
                                );

                                compressor(curMipData, dst, mipW, mipH);
                                prevMipData.swap(std::move(curMipData));
                            }
                            else
                            {
                                const Input::Image& src = input.Get(face, slice, mip);
                                compressor(src.pixels, dst, mipW, mipH);
                            }
                        }
                    }
                }
            }

            return true;
        }

        bool WriteTexture(const Input& input, const Options& options, OutputMemoryStream& outMem)
        {
            // CompiledReource
            // ---------------------------
            // |  TextureHeader          |
            // ---------------------------
            // |  CompressedData         |
            // ---------------------------

            U32 sourceMipLevels = input.mips;
            bool useMipLevels = (options.generateMipmaps || sourceMipLevels > 1) && (input.w > 1 || input.h > 1);
            U32 mipLevels = MipLevelsCount(input.w, input.h, useMipLevels);
            if (useMipLevels && !options.generateMipmaps && mipLevels != sourceMipLevels)
            {
                Logger::Error("Failed to import texture without full mip chain.");
                return false;
            }

            VkFormat format;
            if (!options.compress)
                format = VK_FORMAT_R8G8B8A8_UNORM;
            else if (input.hasAlpha)
                format = VK_FORMAT_BC3_UNORM_BLOCK;
            else
                format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;

            // Write texture header
            TextureHeader header = {};
            header.format = format;
            header.width = input.w;
            header.height = input.h;
            header.mips = mipLevels;
            header.type = TextureResourceType::INTERNAL;
            outMem.Write(header);

            // Write texture data
            if (!options.compress)
                WriteTexture(CompressRGBA, input, options, outMem);
            else if (input.hasAlpha)
                WriteTexture(CompressBC3, input, options, outMem);
            else
                WriteTexture(CompressBC1, input, options, outMem);

            return true;
        }
    }

    namespace TextureImporter
    {
        bool GetImageType(const char* path, ImageType& type)
        {
            char extension[5] = {};
            CopyString(Span(extension), Path::GetExtension(Span(path, StringLength(path))));
            if (EqualString(extension, "tga"))
            {
                type = ImageType::TGA;
            }
            else if (EqualString(extension, "dds"))
            {
                type = ImageType::DDS;
            }
            else if (EqualString(extension, "png"))
            {
                type = ImageType::PNG;
            }
            else if (EqualString(extension, "bmp"))
            {
                type = ImageType::BMP;
            }
            else if (EqualString(extension, "hdr"))
            {
                type = ImageType::HDR;
            }
            else if (EqualString(extension, "jpeg") || EqualString(extension, "jpg"))
            {
                type = ImageType::JPEG;
            }
            else if (EqualString(extension, "raw"))
            {
                type = ImageType::RAW;
            }
            else
            {
                Logger::Warning("Unknown file type.");
                return false;
            }
            return true;
        }

        bool Import(EditorApp& editor, const char* filename, const ImportConfig& config)
        {
            ImageType type;
            if (!GetImageType(filename, type))
                return false;

            FileSystem& fs = editor.GetEngine().GetFileSystem();
            OutputMemoryStream mem;
            if (!fs.LoadContext(filename, mem))
                return false;

            TextureCompressor::Input input;
            switch (type)
            {
            case ImageType::PNG:
            case ImageType::BMP:
            case ImageType::JPEG:
            case ImageType::HDR:
            case ImageType::TGA:
            {
                const bool is16bit = stbi_is_16_bit_from_memory(mem.Data(), (I32)mem.Size());
                if (is16bit)
                {
                    Logger::Warning("16bit images not yet supported. Converting to 8bit, %s", filename);
                    return false;
                }

                int w, h, comps;
                stbi_uc* stbData = stbi_load_from_memory(mem.Data(), (I32)mem.Size(), &w, &h, &comps, 4);
                if (!stbData)
                    return false;

                GPU::TextureFormatLayout layout;
                VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
                layout.SetTexture2D(format, w, h, 1);
                auto blockStride = layout.GetBlockStride();

                // Setup texture input
                input.Init(w, h, format, 1, 1);
                input.Add(Span((const U8*)stbData, w * h * blockStride), 0, 0, 0);
                input.hasAlpha = comps == 4;

                stbi_image_free(stbData);
                break;
            }
            default:
                Logger::Warning("Unknown format.");
                return false;
            }

            OutputMemoryStream outMem;
            TextureCompressor::Options options;
            options.generateMipmaps = config.generateMipmaps;
            options.compress = config.compress;
            if (!TextureCompressor::WriteTexture(input, options, outMem))
                return false;

            return editor.GetAssetCompiler().WriteCompiled(filename, Span(outMem.Data(), outMem.Size()));
        }
    }
}
}