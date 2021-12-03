#pragma once

#include <assert.h>

namespace VulkanTest
{
template<typename T>
class Span
{
public:
	constexpr Span() noexcept = default;
	constexpr Span(T* begin, size_t len)noexcept :
		mBegin(begin),
		mEnd(begin + len)
	{}
	constexpr Span(T& begin)noexcept :
		mBegin(&begin),
		mEnd(mBegin + 1)
	{}
	constexpr Span(T* begin, T* end)noexcept :
		mBegin(begin),
		mEnd(end)
	{}
	template<size_t N>
	constexpr Span(T(&data)[N])noexcept :
		mBegin(data),
		mEnd(data + N)
	{}

	constexpr operator Span<const T>() const noexcept
	{
		return Span<const T>(mBegin, mEnd);
	}

	constexpr T& operator[](const size_t idx) const noexcept
	{
		assert(mBegin + idx < mEnd);
		return mBegin[idx];
	}

	constexpr bool empty()const noexcept {
		return length() <= 0;
	}

	constexpr size_t length()const noexcept {
		return size_t(mEnd - mBegin);
	}

	constexpr T* begin()const noexcept {
		return mBegin;
	}

	constexpr T* end()const noexcept {
		return mEnd;
	}

	constexpr T* data()const noexcept {
		return mBegin;
	}

private:
	T* mBegin = nullptr;
	T* mEnd   = nullptr;
};
}