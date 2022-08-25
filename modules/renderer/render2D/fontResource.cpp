#include "fontResource.h"
#include "font.h"
#include "core\engine.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H            // <freetype/ftmodapi.h>
#include FT_GLYPH_H             // <freetype/ftglyph.h>
#include FT_SYNTHESIS_H         // <freetype/ftsynth.h>

namespace VulkanTest
{
	DEFINE_RESOURCE(FontResource);

	FontResource::FontResource(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_),
		face(nullptr)
	{
	}

	FontResource::~FontResource()
	{
		ASSERT(IsEmpty());
	}

	Font* FontResource::GetFont(I32 size)
	{
		PROFILE_FUNCTION();

		if (!WaitForLoaded())
			return nullptr;

		ScopedMutex lock(mutex);
		if (face == nullptr)
			return nullptr;

		for (auto font : fonts)
		{
			if (font->GetResource() == this && font->GetSize() == size)
				return font;
		}

		return CJING_NEW(Font)(this, size);
	}

	bool FontResource::Init(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
		inputMem.Read<FontHeader>(header);
		if (header.magic != FontHeader::MAGIC)
		{
			Logger::Warning("Unsupported font file %s", GetPath());
			return false;
		}

		if (header.version != FontHeader::LAST_VERSION)
		{
			Logger::Warning("Unsupported version of font %s", GetPath());
			return false;
		}
		return true;
	}

	bool FontResource::Load()
	{
		PROFILE_FUNCTION();
		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;

		FontResourceFactory* factory = static_cast<FontResourceFactory*>(&GetResourceFactory());
		const FT_Error error = FT_New_Memory_Face(factory->GetLibrary(), dataChunk->Data(), static_cast<FT_Long>(dataChunk->Size()), 0, face);
		if (error)
		{
			return false;
		}

		return true;
	}

	void FontResource::Unload()
	{
	}

	static void* FreeTypeAlloc(FT_Memory memory, long size)
	{
		return CJING_MALLOC(size);
	}

	void* FreeTypeRealloc(FT_Memory memory, long oldSize, long newSize, void* ptr)
	{
		return CJING_REMALLOC(ptr, newSize);
	}

	void FreeTypeFree(FT_Memory memory, void* ptr)
	{
		return CJING_FREE(ptr);
	}

	FT_Library Library;
	FT_MemoryRec_ FreeTypeMemory;

	class FontManagerServiceImpl : public EngineService
	{
	public:
		FontManagerServiceImpl() :
			EngineService("FontManagerServiceImpl", -700)
		{}

		~FontManagerServiceImpl()
		{
		}

		bool Init() override
		{
			ASSERT(Library == nullptr);

			FreeTypeMemory.alloc = &FreeTypeAlloc;
			FreeTypeMemory.realloc = &FreeTypeRealloc;
			FreeTypeMemory.free = &FreeTypeFree;
			const FT_Error error = FT_New_Library(&FreeTypeMemory, &Library);
			if (error)
			{
				Logger::Error("Failed to init freetype %x", error);
				return false;
			}

			FT_Add_Default_Modules(Library);

			FT_Int major, minor, patch;
			FT_Library_Version(Library, &major, &minor, &patch);
			Logger::Info("Freetype initialized %d.%d.%d", major, minor, patch);

			initialized = true;
			return true;
		}

		void Uninit() override
		{
			if (Library)
			{
				const FT_Error error = FT_Done_Library(Library);
				Library = nullptr;
				if (error)
				{
					Logger::Error("Failed to uninit freetype %x", error);
				}
			}
			initialized = false;
		}
	};
	FontManagerServiceImpl FontManagerServiceImplInstance;

	FontResourceFactory::FontResourceFactory()
	{
	}

	FontResourceFactory::~FontResourceFactory()
	{

	}

	FT_Library FontResourceFactory::GetLibrary()
	{
		return Library;
	}

	Resource* FontResourceFactory::CreateResource(const Path& path)
	{
		return CJING_NEW(FontResource)(path, *this);
	}
}