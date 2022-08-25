#pragma once

#include "core\common.h"
#include "core\resource\binaryResource.h"
#include "core\profiler\profiler.h"

typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_* FT_Library;

namespace VulkanTest
{
	class Font;

	class VULKAN_TEST_API FontResource final : public BinaryResource
	{
	public:
		DECLARE_RESOURCE(FontResource);

#pragma pack(1)
		struct FontHeader
		{
			static constexpr U32 LAST_VERSION = 0;
			static constexpr U32 MAGIC = '_CST';
			U32 magic = MAGIC;
			U32 version = LAST_VERSION;
		};
#pragma pack()

		FontResource(const Path& path_, ResourceFactory& resFactory_);
		virtual ~FontResource();

		Font* GetFont(I32 size);

	protected:
		bool Init(ResourceInitData& initData)override;
		bool Load()override;
		void Unload() override;

	private:
		FontHeader header;
		FT_Face* face;
		Array<Font*> fonts;
		Mutex mutex;
	};

	class VULKAN_TEST_API FontResourceFactory final : public BinaryResourceFactory
	{
	public:
		FontResourceFactory();
		virtual ~FontResourceFactory();

		FT_Library GetLibrary();

	protected:
		Resource* CreateResource(const Path& path) override;

	};
}