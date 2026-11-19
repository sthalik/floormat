#pragma once
#include <type_traits>
#include <cr/Macros.h>

namespace floormat::detail {

struct move_fn
{
    template<typename T>
    static constexpr CORRADE_ALWAYS_INLINE std::remove_reference_t<T>&& operator()(T&& t) noexcept;
};

template<typename T>
struct forward_fn
{
    static constexpr CORRADE_ALWAYS_INLINE T&& operator()(std::remove_reference_t<T>& t) noexcept;
    static constexpr CORRADE_ALWAYS_INLINE T&& operator()(std::remove_reference_t<T>&& t) noexcept;
};

// from Corrade/Utility/Move.h

template<typename T>
constexpr std::remove_reference_t<T>&& move_fn::operator()(T&& t) noexcept
{
    return static_cast<std::remove_reference_t<T>&&>(t);
}

template<typename T>
constexpr T&& forward_fn<T>::operator()(std::remove_reference_t<T>& t) noexcept
{
    return static_cast<T&&>(t);
}

template<typename T>
constexpr T&& forward_fn<T>::operator()(std::remove_reference_t<T>&& t) noexcept
{
    static_assert(!std::is_lvalue_reference_v<T>);
    return static_cast<T&&>(t);
}

} // namespace floormat::detail

namespace floormat {

template<typename T> concept AlwaysTrue = true;

constexpr inline detail::move_fn move{};
template<typename T> constexpr inline detail::forward_fn<T> forward{};

template<AlwaysTrue T>
constexpr inline void swap(T& a, std::common_type_t<T>& b)
noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
{
    T tmp = static_cast<T&&>(a);
    a = static_cast<T&&>(b);
    b = static_cast<T&&>(tmp);
}

} // namespace floormat
