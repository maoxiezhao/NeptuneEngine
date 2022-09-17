#pragma once

#include <assert.h>

template<typename T>
class Span
{
public:
	constexpr Span() noexcept = default;
	constexpr Span(T* begin, size_t len)noexcept :
		pBegin(begin),
		pEnd(begin + len)
	{}
	constexpr Span(T& begin)noexcept :
		pBegin(&begin),
		pEnd(pBegin + 1)
	{}
	constexpr Span(T* begin, T* end)noexcept :
		pBegin(begin),
		pEnd(end)
	{}
	template<size_t N>
	constexpr Span(T(&data)[N])noexcept :
		pBegin(data),
		pEnd(data + N)
	{}

	constexpr operator Span<const T>() const noexcept
	{
		return Span<const T>(pBegin, pEnd);
	}

	constexpr T& operator[](const size_t idx) const noexcept
	{
		assert(pBegin + idx < pEnd);
		return pBegin[idx];
	}

	constexpr bool empty()const noexcept {
		return length() <= 0;
	}

	constexpr size_t length()const noexcept {
		return size_t(pEnd - pBegin);
	}

	constexpr T* data()const noexcept {
		return pBegin;
	}

	// Support foreach
	constexpr T* begin()const noexcept { return pBegin; }
	constexpr T* end()const noexcept { return pEnd; }

public:
	T* pBegin = nullptr;
	T* pEnd = nullptr;
};