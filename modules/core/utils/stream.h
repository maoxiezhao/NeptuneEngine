#pragma once

#include "core\common.h"

namespace VulkanTest
{
    struct VULKAN_TEST_API IInputStream 
    {
        virtual bool Read(void* buffer, U64 size) = 0;
        virtual const void* GetBuffer() const = 0;
        virtual U64 Size() const = 0;

		template <typename T> 
		void Read(T& value) {
			Read(&value, sizeof(T)); 
		}
		template <typename T> T Read();
    };

    struct VULKAN_TEST_API IOutputStream
    {
        virtual bool Write(const void* buffer, U64 size) = 0;

		template <typename T> 
		bool Write(const T& value)
		{
			return Write(&value, sizeof(T));
		}

		IOutputStream& operator<<(const char* str);
		IOutputStream& operator<<(I32 data);
		IOutputStream& operator<<(I64 data);
		IOutputStream& operator<<(U32 data);
		IOutputStream& operator<<(U64 data);
		IOutputStream& operator<<(F32 data);
		IOutputStream& operator<<(F64 data);
    };

	template <>
	inline bool IOutputStream::Write<bool>(const bool& value)
	{
		U8 v = value;
		return Write(&v, sizeof(v));
	}

	struct VULKAN_TEST_API OutputMemoryStream final : public IOutputStream
	{
	public:
		OutputMemoryStream();
		OutputMemoryStream(void* data_, U64 size_);
		OutputMemoryStream(OutputMemoryStream&& rhs);
		OutputMemoryStream(const OutputMemoryStream& rhs);
		~OutputMemoryStream();

		void operator =(const OutputMemoryStream& rhs);
		void operator =(OutputMemoryStream&& rhs);

		template <typename T>
		bool Write(const T& value)
		{
			return Write(&value, sizeof(T));
		}
		void WriteString(const char* string);
		bool Write(const void* buffer, U64 size_)override;
		bool Write(const void* buffer, U64 size_, U64 alignment);
		void Skip(U64 size_);
		void Resize(U64 newSize);
		void Reserve(U64 newSize);
		void Clear();
		void Free();

		bool Empty()const {
			return size == 0;
		}
		U64 Size()const {
			return size;
		}
		U64 Capacity()const {
			return capacity;
		}
		U8* Data() {
			return data;
		}
		const U8* Data() const {
			return data;
		}

		operator Span<const U8>() const { return Span(data, data + size); }

	private:
		U8* data;
		U64 capacity;
		U64 size;
	};

	struct VULKAN_TEST_API InputMemoryStream final : public IInputStream
	{
		InputMemoryStream(const void* data_, U64 size_);
		explicit InputMemoryStream(const OutputMemoryStream& blob);

		bool Read(void* buffer_, U64 size_) override;
		const char* ReadString();

		const void* GetBuffer() const override {
			return data;
		}

		U64 Size() const override {
			return size;
		}
		U64 GetPos()const {
			return pos;
		}
		void SetPos(U64 pos_) {
			pos = pos_;
		}

		using IInputStream::Read;

	private:
		const U8* data;
		U64 size;
		U64 pos;
	};

	template <typename T> 
	T IInputStream::Read()
	{
		T v;
		Read(&v, sizeof(v));
		return v;
	}

	template <>
	inline bool IInputStream::Read<bool>()
	{
		U8 v;
		Read(&v, sizeof(v));
		return v != 0;
	}
} 
