#pragma once
#include <cstddef>
#include <limits>

namespace floormat::entities::erased_constraints {

static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t));
static_assert(sizeof(std::size_t) == sizeof(std::ptrdiff_t));

struct range final
{
    using U = std::size_t;
    using I = std::make_signed_t<U>;
    enum type_ : unsigned char { type_none, type_float, type_uint, type_int, };
    union element {
        float f;
        U u;
        I i;
    };

    element min {.i = 0}, max {.i = 0};
    type_ type = type_none;

    template<typename T> constexpr std::pair<T, T> convert() const;
};

template<typename T> constexpr std::pair<T, T> range::convert() const
{
    if constexpr (!std::is_floating_point_v<T> && !std::is_integral_v<T>)
        return {{}, {}};

    using std::size_t;
    static_assert(sizeof(T) <= sizeof(size_t) || !std::is_integral_v<T>);

    constexpr auto min_ = []<typename V>(V a, V b) { return a < b ? a : b; };
    constexpr auto max_ = []<typename V>(V a, V b) { return a > b ? a : b; };
    using limits = std::numeric_limits<T>;
    constexpr auto lmin = limits::min(), lmax = limits::max();

    switch (type) {
    case type_float:
        if constexpr (limits::is_integer())
            return { T(std::floor(min.f)), T(std::ceil(max.f)) };
        else
            return { T(min.f), T(max.f) };
    case type_uint:  return { max_(T(min.u), lmin), T(min_(size_t(max.u), size_t(lmax))) };
    case type_int:   return { max_(T(min.i), lmin), T(min_(size_t(max.i), size_t(lmax))) };
    default: case type_none:  return { lmin, lmax };
    }
}

struct length final {
    std::size_t value = std::numeric_limits<std::size_t>::max();
};

struct group final {
    StringView group_name;
};

} // namespace floormat::entities::erased_constraints
