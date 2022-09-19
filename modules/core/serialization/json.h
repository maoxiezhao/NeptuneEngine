#pragma once

#include "core\common.h"
#include "core\utils\string.h"
#include "core\memory\memory.h"

#define RAPIDJSON_ERROR_CHARTYPE char
#define RAPIDJSON_ERROR_STRING(x) TEXT(x)
#define RAPIDJSON_ASSERT(x) ASSERT(x)
#define RAPIDJSON_NEW(x) VulkanTest::New<x>()
#define RAPIDJSON_DELETE(x) VulkanTest::Delete(x)
#define RAPIDJSON_NOMEMBERITERATORCLASS
#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>

namespace rapidjson_flax
{
    using namespace VulkanTest;

    class EngineAllocator
    {
    public:
        static const bool kNeedFree = true;

        void* Malloc(size_t size)
        {
            // Behavior of malloc(0) is implementation defined so for size=0 return nullptr.
            // By default Flax doesn't use Allocate(0) so it's not important for the engine itself.
            if (size)
                return CJING_MALLOC(size);
            return nullptr;
        }

        void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
        {
            return CJING_REMALLOC(originalPtr, newSize);
        }

        static void Free(void* ptr)
        {
            CJING_FREE(ptr);
        }
    };

    // String buffer with UTF8 encoding
    typedef rapidjson::GenericStringBuffer<rapidjson::UTF8<>, EngineAllocator> StringBuffer;

    // GenericDocument with UTF8 encoding
    typedef rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<EngineAllocator>, EngineAllocator> Document;

    // GenericValue with UTF8 encoding
    typedef rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::MemoryPoolAllocator<EngineAllocator>> Value;

    // JSON writer to the stream
    template<typename OutputStream, typename SourceEncoding = rapidjson::UTF8<>, typename TargetEncoding = rapidjson::UTF8<>, unsigned writeFlags = rapidjson::kWriteDefaultFlags>
    using Writer = rapidjson::Writer<OutputStream, SourceEncoding, TargetEncoding, EngineAllocator, writeFlags>;

    // Pretty JSON writer to the stream
    template<typename OutputStream, typename SourceEncoding = rapidjson::UTF8<>, typename TargetEncoding = rapidjson::UTF8<>, unsigned writeFlags = rapidjson::kWriteDefaultFlags>
    using PrettyWriter = rapidjson::PrettyWriter<OutputStream, SourceEncoding, TargetEncoding, EngineAllocator, writeFlags>;
}
