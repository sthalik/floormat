#pragma once
#include "integer-types.hpp"
#include <type_traits>
#include <version>
#ifdef __cpp_lib_start_lifetime_as
#include <memory>
#else
#include <cstring>
#include <new>
#endif

namespace floormat::detail {

template<typename T>
struct start_lifetime_as_array_fn
{
    static T* operator()(void* p, usize n) noexcept;
};

template<typename T>
T* start_lifetime_as_array_fn<T>::operator()(void* p, usize n) noexcept
{
    static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>);
#ifdef __cpp_lib_start_lifetime_as
    return std::start_lifetime_as_array<T>(p, n);
#else
    // P0593R6: memmove implicitly creates objects in the destination and keeps its bytes.
    // std::launder needs an object at p, which an empty range lacks.
    if (n == 0)
        return static_cast<T*>(p);
    else
        return std::launder(static_cast<T*>(std::memmove(p, p, n * sizeof(T))));
#endif
}

} // namespace floormat::detail

namespace floormat {

template<typename T> constexpr inline detail::start_lifetime_as_array_fn<T> start_lifetime_as_array{};

} // namespace floormat
