#pragma once

#include "json.h"
#include "stream.h"
#include "core\types\guid.h"
#include "core\utils\string.h"
#include "core\scene\world.h"
#include "math\geometry.h"

namespace VulkanTest
{
#define JKEY(keyname) Key(keyname, ARRAYSIZE(keyname) - 1)

	class VULKAN_TEST_API JsonWriterBase
	{
	public:
        JsonWriterBase() = default;
		~JsonWriterBase() = default;

        JsonWriterBase(const JsonWriterBase& rhs) = delete;
		void operator=(const JsonWriterBase& rhs) = delete;

		virtual void StartObject() = 0;
		virtual void EndObject() = 0;
		virtual void StartArray() = 0;
		virtual void EndArray(I32 count = 0) = 0;

		virtual void Key(const char* str, I32 length) = 0;
		virtual void String(const char* str, I32 length) = 0;
		virtual void Bool(bool d) = 0;
		virtual void Int(I32 d) = 0;
		virtual void Int64(I64 d) = 0;
		virtual void Uint(U32 d) = 0;
		virtual void Uint64(U64 d) = 0;
		virtual void Float(F32 d) = 0;
		virtual void Double(F64 d) = 0;

		FORCE_INLINE void Key(const char* str) {
			Key(str, (I32)StringLength(str));
		}

		FORCE_INLINE void String(const char* str) {
			String(str, (I32)StringLength(str));
		}

        FORCE_INLINE void String(const VulkanTest::String& str) {
            String(str.c_str(), (I32)str.size());
        }

        void Guid(const Guid& guid);
        void Entity(const ECS::Entity& entity);

        void Float2(const F32x2& value);
        void Float3(const F32x3& value);
        void Float4(const F32x4& value);
        void WriteAABB(const AABB& aabb);
    };

    template<typename WriterType>
    class JsonWriterType : public JsonWriterBase
    {
    protected:
        WriterType writer;

    public:
        using JsonWriterBase::String;

        JsonWriterType(rapidjson_flax::StringBuffer& buffer)
            : JsonWriterBase()
            , writer(buffer)
        {
        }

        FORCE_INLINE WriterType& GetWriter() {
            return writer;
        }

    public:
        void Key(const char* str, I32 length) override
        {
            writer.Key(str, static_cast<rapidjson::SizeType>(length));
        }

        void String(const char* str, I32 length) override
        {
            writer.String(str, length);
        }

        void Bool(bool d) override
        {
            writer.Bool(d);
        }

        void Int(I32 d) override
        {
            writer.Int(d);
        }

        void Int64(I64 d) override
        {
            writer.Int64(d);
        }

        void Uint(U32 d) override
        {
            writer.Uint(d);
        }

        void Uint64(U64 d) override
        {
            writer.Uint64(d);
        }

        void Float(F32 d) override
        {
            writer.Float(d);
        }

        void Double(F64 d) override
        {
            writer.Double(d);
        }

        void StartObject() override
        {
            writer.StartObject();
        }

        void EndObject() override
        {
            writer.EndObject();
        }

        void StartArray() override
        {
            writer.StartArray();
        }

        void EndArray(I32 count = 0) override
        {
            writer.EndArray(count);
        }
    };

    class VULKAN_TEST_API CompactJsonWriterImpl : public rapidjson_flax::Writer<rapidjson_flax::StringBuffer>
    {
    public:
        CompactJsonWriterImpl(rapidjson_flax::StringBuffer& buffer)
            : Writer(buffer)
        {
        }

        void RawValue(const char* json, I32 length)
        {
            Prefix(rapidjson::kObjectType);
            WriteRawValue(json, length);
        }

        void Float(float d)
        {
            Prefix(rapidjson::kNumberType);
            WriteDouble(d);
        }
    };

    class VULKAN_TEST_API PrettyJsonWriterImpl : public rapidjson_flax::PrettyWriter<rapidjson_flax::StringBuffer>
    {
    public:
        using Writer = rapidjson_flax::PrettyWriter<rapidjson_flax::StringBuffer> ;

        PrettyJsonWriterImpl(rapidjson_flax::StringBuffer& buffer)
            : Writer(buffer)
        {
            SetIndent('\t', 1);
        }

        void RawValue(const char* json, I32 length)
        {
            Prefix(rapidjson::kObjectType);
            WriteRawValue(json, length);
        }

        void Float(float d)
        {
            Prefix(rapidjson::kNumberType);
            WriteDouble(d);
        }
    };

    using JsonWriter = JsonWriterType<PrettyJsonWriterImpl>;
}