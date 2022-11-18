#pragma once

#include <cstddef>
#include <cstdint>
#include <bit>
#include <concepts>
#include <type_traits>

namespace Corrade::Containers {

template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;

} // namespace Corrade::Containers

namespace floormat::Serialize {

static_assert(std::endian::native == std::endian::big || std::endian::native == std::endian::little);

enum class value_type : unsigned char {
    none, uc, u8, u16, u32, u64,
    f32, f64,
    COUNT
};

template<std::size_t N> struct make_integer;
template<std::size_t N> using make_integer_t = typename make_integer<N>::type;

#define FM_SERIALIZE_MAKE_INTEGER(T) template<> struct make_integer<sizeof(T)> { using type = T; }
FM_SERIALIZE_MAKE_INTEGER(std::uint8_t);
FM_SERIALIZE_MAKE_INTEGER(std::uint16_t);
FM_SERIALIZE_MAKE_INTEGER(std::uint32_t);
FM_SERIALIZE_MAKE_INTEGER(std::uint64_t);
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
constexpr inline T maybe_byteswap(T x)
{
    return x;
}

template<integer T>
constexpr inline T maybe_byteswap(T x)
{
    if constexpr(std::endian::native == std::endian::big)
        return std::byteswap(x);
    else
        return x;
}

} // namespace floormat::Serialize
