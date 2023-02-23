#pragma once
#include <Corrade/Containers/StringView.h>

namespace floormat::entities::erased_constraints {

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

    template<typename T> std::pair<T, T> convert() const;
    friend bool operator==(const range& a, const range& b);
};

struct max_length final {
    std::size_t value = std::size_t(-1);
    constexpr operator std::size_t() const { return value; }
};

struct group final {
    StringView group_name;
    constexpr operator StringView() const { return group_name; }
    constexpr group() = default;
    constexpr group(StringView name) : group_name{name} {}
};

} // namespace floormat::entities::erased_constraints
