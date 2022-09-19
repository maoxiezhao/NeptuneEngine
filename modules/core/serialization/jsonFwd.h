#pragma once

namespace rapidjson
{
    template<typename BaseAllocator>
    class MemoryPoolAllocator;
    template<typename CharType>
    struct UTF8;
    template<typename OutputStream, typename SourceEncoding, typename TargetEncoding, typename StackAllocator, unsigned writeFlags>
    class Writer;
    template<typename OutputStream, typename SourceEncoding, typename TargetEncoding, typename StackAllocator, unsigned writeFlags>
    class PrettyWriter;
    template<typename Encoding, typename Allocator>
    class GenericStringBuffer;
    template<typename Encoding, typename Allocator>
    class GenericValue;
    template<typename Encoding, typename Allocator, typename StackAllocator>
    class GenericDocument;
}

namespace rapidjson_flax
{
    class EngineAllocator;
    typedef rapidjson::GenericStringBuffer<rapidjson::UTF8<char>, EngineAllocator> StringBuffer;
    typedef rapidjson::GenericDocument<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<EngineAllocator>, EngineAllocator> Document;
    typedef rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<EngineAllocator>> Value;
}