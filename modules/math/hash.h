#pragma once

#include "math_common.h"

#include <functional>
#include <type_traits>

namespace VulkanTest
{
// SDBM Hash
inline uint32_t SDBMHash(uint32_t hash, unsigned char c)
{
	return c + (hash << 6) + (hash << 16) - hash;
}

inline uint32_t SDBHash(uint32_t input, const void* inData, size_t size)
{
	const uint8_t* data = static_cast<const uint8_t*>(inData);
	uint32_t hash = input;
	while (size--) {
		hash = ((uint32_t)*data++) + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

// FNV-1a Hash
uint64_t FNV1aHash(uint64_t input, char c);
uint64_t FNV1aHash(uint64_t input, const void* data, size_t size);

template<typename T>
inline void HashCombine(uint32_t& seed, const T& value)
{
	std::hash<T> hasher;
	seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<typename T>
inline std::enable_if_t<std::is_enum<T>::value, uint64_t> HashFunc(uint64_t Input, const T& Data) {
	int32_t value = (int32_t)Data;
	return FNV1aHash(Input, &value, 4);
}

// Hasher
template<typename T>
inline std::enable_if_t<!std::is_enum<T>::value, uint64_t> HashFunc(uint64_t Input, const T& Data)
{
	static_assert(false, "This Type of Hash function not defined");
	return 0;
}

uint64_t HashFunc(uint64_t Input, const char* Data);
inline uint64_t HashFunc(uint64_t Input, uint32_t Data) { return FNV1aHash(Input, &Data, 4); }
inline uint64_t HashFunc(uint64_t Input, uint64_t Data) { return FNV1aHash(Input, &Data, sizeof(Data)); }
inline uint64_t HashFunc(uint64_t Input, int32_t Data) { return FNV1aHash(Input, &Data, 4); }
inline uint64_t HashFunc(uint64_t Input, int64_t Data) { return FNV1aHash(Input, &Data, sizeof(Data)); }

inline uint64_t HashFunc(uint64_t Input, float Data)
{
	union
	{
		float f32;
		uint32_t u32;
	} u;
	u.f32 = Data;
	return HashFunc(Input, u.u32);
}

template<typename T>
inline uint32_t HashFunc(uint32_t Input, const T& Data)
{
	static_assert(false, "This Type of Hash function not defined");
	return 0;
}
inline uint32_t HashFunc(uint32_t Input, uint32_t Data) { return SDBHash(Input, &Data, 4); }
inline uint32_t HashFunc(uint32_t Input, int32_t Data) { return SDBHash(Input, &Data, 4); }

// Use XXHash algorithm with high performance
uint64_t XXHash64(const void* input, size_t length);

template<typename T>
class Hasher
{
public:
	static U32 Get(const T& key) { return HashFunc((U32)0, key); }
};

using HashValue = uint64_t;

class HashCombiner
{
private:
	HashValue mHashValue = 0xcbf29ce484222325ull;

public:
	explicit HashCombiner(HashValue h) : mHashValue(h) {}
	HashCombiner() = default;

	inline HashValue Get() const
	{
		return mHashValue;
	}

	template<typename T>
	inline void HashCombine(T v)
	{
		mHashValue = HashFunc(mHashValue, v);
	}

	template <typename T>
	inline void HashCombine(const T* data_, size_t size)
	{
		size /= sizeof(*data_);
		for (size_t i = 0; i < size; i++)
			HashCombine<T>(data_[i]);
	}
};

template <typename T>
class HashedObject
{
public:
	HashedObject() = default;
	HashedObject(HashValue hash)
		: hashValue(hash)
	{
	}

	void SetHash(HashValue hash)
	{
		hashValue = hash;
	}

	HashValue GetHash() const
	{
		return hashValue;
	}

private:
	HashValue hashValue = 0;
};

struct RuntimeHash 
{
	static RuntimeHash FromU64(U64 hash);
	RuntimeHash() {}
	explicit RuntimeHash(const char* str);
	RuntimeHash(const void* data, U32 len);

	bool operator != (RuntimeHash rhs) const { return hash != rhs.hash; }
	bool operator == (RuntimeHash rhs) const { return hash == rhs.hash; }

	U64 GetHashValue() const { return hash; }

private:
	U64 hash = 0;
};
}