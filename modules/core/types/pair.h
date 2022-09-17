#pragma once

#include <type_traits>

template<typename T, typename U>
struct Pair
{
public:
    T first;
    U second;

public:
    Pair()
    {
    }

    Pair(const T& key, const U& value)
        : first(key)
        , second(value)
    {
    }

    Pair(const Pair& other)
        : first(other.first)
        , second(other.second)
    {
    }

    Pair(Pair&& other) noexcept
        : first(std::move(other.first))
        , second(std::move(other.second))
    {
    }

public:
    Pair& operator=(const Pair& other)
    {
        if (this == &other)
            return *this;
        first = other.first;
        second = other.second;
        return *this;
    }

    Pair& operator=(Pair&& other) noexcept
    {
        if (this != &other)
        {
            first = std::move(other.first);
            second = std::move(other.second);
        }
        return *this;
    }

    friend bool operator==(const Pair& a, const Pair& b)
    {
        return a.first == b.first && a.second == b.second;
    }

    friend bool operator!=(const Pair& a, const Pair& b)
    {
        return a.first != b.first || a.second != b.second;
    }
};

template<class T, class U>
inline constexpr Pair<T, U> ToPair(const T& key, const U& value)
{
    return Pair<T, U>(key, value);
}
