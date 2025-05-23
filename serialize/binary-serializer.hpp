#pragma once

#include <bit>
#include <concepts>
#include <type_traits>

namespace floormat::Serialize {

static_assert(std::endian::native == std::endian::big || std::endian::native == std::endian::little);

template<size_t N> struct make_integer;
template<size_t N> using make_integer_t = typename make_integer<N>::type;

#define FM_SERIALIZE_MAKE_INTEGER(T) template<> struct make_integer<sizeof(T)> { using type = T; }
FM_SERIALIZE_MAKE_INTEGER(uint8_t);
FM_SERIALIZE_MAKE_INTEGER(uint16_t);
FM_SERIALIZE_MAKE_INTEGER(uint32_t);
FM_SERIALIZE_MAKE_INTEGER(uint64_t);
#undef FN_SERIALIZE_MAKE_INTEGER

template<typename T>
concept integer = requires(T x) {
    requires std::integral<T>;
    requires sizeof(T) == sizeof(make_integer_t<sizeof(T)>);
};

template<typename T>
concept serializable = requires(T x) {
    requires std::same_as<T, std::decay_t<T>>;
    requires std::floating_point<T> || integer<T>;
};

template<typename T>
constexpr CORRADE_ALWAYS_INLINE T maybe_byteswap(T x) noexcept
{
    return x;
}

template<integer T>
requires (sizeof(T) > 1)
constexpr CORRADE_ALWAYS_INLINE T maybe_byteswap(T x) noexcept
{
    if constexpr(std::endian::native == std::endian::big)
        return std::byteswap(x);
    else
        return x;
}

} // namespace floormat::Serialize
