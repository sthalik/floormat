#pragma once
#include <type_traits>

namespace floormat {

template<typename T> concept AlwaysTrue = true;

// from Corrade/Utility/Move.h

template<AlwaysTrue T>
constexpr T&& forward(std::remove_reference_t<T>& t) noexcept
{
    return static_cast<T&&>(t);
}

template<AlwaysTrue T>
constexpr T&& forward(std::remove_reference_t<T>&& t) noexcept
{
    static_assert(!std::is_lvalue_reference_v<T>);
    return static_cast<T&&>(t);
}

template<AlwaysTrue T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept
{
    return static_cast<std::remove_reference_t<T>&&>(t);
}

template<AlwaysTrue T>
void swap(T& a, std::common_type_t<T>& b)
noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
{
    T tmp = static_cast<T&&>(a);
    a = static_cast<T&&>(b);
    b = static_cast<T&&>(tmp);
}

} // namespace floormat
