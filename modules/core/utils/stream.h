#pragma once

#include "common.h"

namespace VulkanTest
{
    struct VULKAN_TEST_API IInputStream 
    {
        virtual bool Read(void* buffer, U64 size) = 0;
        virtual const void* GetBuffer() const = 0;
        virtual U64 Size() const = 0;
        
        template <typename T>
        void Read(T& value) 
        { 
			Read(&value, sizeof(T));
        }

		template <typename T> T Read()
		{
			T v;
			Read(&v, sizeof(v));
			return v;
		}
    };

    template <>
    inline bool IInputStream::Read<bool>()
    {
        U8 v;
        Read(&v, sizeof(v));
        return v != 0;
    }

    struct VULKAN_TEST_API IOutputStream
    {
        virtual bool Write(const void* buffer, U64 size) = 0;

		template <typename T> 
		bool Write(const T& value)
		{
			return Write(&value, sizeof(T));
		}

		inline IOutputStream& operator<<(bool data)
		{
			Write((uint32_t)(data ? 1 : 0));
			return *this;
		}
		inline IOutputStream& operator<<(char data)
		{
			Write((int8_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(unsigned char data)
		{
			Write((uint8_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(int data)
		{
			Write((int64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(unsigned int data)
		{
			Write((uint64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(long data)
		{
			Write((int64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(unsigned long data)
		{
			Write((uint64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(long long data)
		{
			Write((int64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(unsigned long long data)
		{
			Write((uint64_t)data);
			return *this;
		}
		inline IOutputStream& operator<<(float data)
		{
			Write(data);
			return *this;
		}
		inline IOutputStream& operator<<(double data)
		{
			Write(data);
			return *this;
		}
    };

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

		bool Write(const void* buffer, U64 size)override;

		void Resize(U64 size_);
		void Reserve(U64 size_);
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
			return buffer;
		}

	private:
		U8* buffer;
		U64 capacity;
		U64 size;
	};
} 
