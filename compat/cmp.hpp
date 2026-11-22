#pragma once
#include <cr/Macros.h>

namespace floormat::cmp {

template<typename T = void>
struct less
{
    static constexpr CORRADE_ALWAYS_INLINE bool operator()(const T& a, const T& b) { return a < b; }
};

template<>
struct less<void>
{
    using is_transparent = void;

    template<typename T, typename U>
    static constexpr CORRADE_ALWAYS_INLINE auto operator()(const T& a, const U& b) -> decltype(a < b) { return a < b; }
};

template<typename T = void>
struct less_equal
{
    static constexpr CORRADE_ALWAYS_INLINE bool operator()(const T& a, const T& b) { return a <= b; }
};

template<>
struct less_equal<void>
{
    using is_transparent = void;

    template<typename T, typename U>
    static constexpr CORRADE_ALWAYS_INLINE auto operator()(const T& a, const U& b) -> decltype(a <= b) { return a <= b; }
};

} // namespace floormat::cmp
