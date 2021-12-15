#pragma once

#include "common.h"

namespace VulkanTest
{
    struct IInputStream 
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

    struct IOutputStream 
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
} 
