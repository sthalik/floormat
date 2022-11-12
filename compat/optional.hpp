#pragma once
#include <type_traits>
#include <Corrade/Containers/Optional.h>

namespace std {
template<class T> struct tuple_size<Corrade::Containers::Optional<T>> : std::integral_constant<std::size_t, 2> {};
template<class T> struct tuple_element<0, Corrade::Containers::Optional<T>> { using type = T; };
template<class T> struct tuple_element<1, Corrade::Containers::Optional<T>> { using type = bool; };
}

namespace Corrade::Containers {

template<std::size_t N, class T>
std::tuple_element_t<N, Optional<T>>
get(const Optional<T>& value) noexcept(std::is_nothrow_default_constructible_v<T> && std::is_nothrow_copy_constructible_v<T>)
{
    static_assert(N < 2);
    static_assert(std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>);
    if constexpr (N == 0)
        return value ? *value : T{};
    if constexpr (N == 1)
        return bool(value);
}

} // namespace Corrade::Containers
