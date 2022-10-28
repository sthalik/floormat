#pragma once
#include "compat/integer-types.hpp"
#include "compat/defs.hpp"

#include <bit>
#include <iterator>
#include <concepts>
#include <type_traits>

namespace Corrade::Containers {

template<typename T> class BasicStringView;
using StringView = BasicStringView<const char>;

} // namespace Corrade::Containers

namespace floormat::Serialize {

static_assert(std::endian::native == std::endian::big || std::endian::native == std::endian::little);

enum class value_type : std::uint8_t {
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

union value_u {
    alignas(alignof(double)) char bytes[8];
    unsigned char uc;
    std::uint8_t u8;
    std::uint16_t u16;
    std::uint32_t u32;
    std::uint64_t u64;
    float f32;
    double f64;
};

static_assert(sizeof(value_u) == 8);

template<typename T>
concept char_sequence = requires(T& x, const T& cx) {
    requires std::same_as<decltype(std::begin(x)), decltype(std::end(x))>;
    requires std::same_as<decltype(std::cbegin(cx)), decltype(std::cend(cx))>;
    requires std::forward_iterator<decltype(std::begin(x))>;
    requires std::forward_iterator<decltype(std::cbegin(cx))>;
    requires std::same_as<char, std::decay_t<decltype(*std::begin(x))>>;
    requires std::same_as<char, std::decay_t<decltype(*std::cbegin(x))>>;
};

template<typename It>
concept string_input_iterator = requires(It it) {
    requires std::forward_iterator<It>;
    requires std::is_same_v<char, std::decay_t<decltype(*it)>>;
};

template<typename T>
concept serializable = requires(T x) {
    requires std::floating_point<T> || integer<T>;
};

template<string_input_iterator It>
struct binary_reader final {
    template<char_sequence Seq> explicit constexpr binary_reader(const Seq& seq) noexcept;
    constexpr binary_reader(It begin, It end) noexcept;
    constexpr ~binary_reader() noexcept;

    template<integer T> constexpr value_u read_u() noexcept;
    template<std::floating_point T> constexpr value_u read_u() noexcept;
    template<typename T> T read() noexcept;

private:
    It it, end;
};

template<string_input_iterator It, serializable T>
binary_reader<It>& operator>>(binary_reader<It>& reader, T& x) noexcept;

template<string_input_iterator It> binary_reader(It&& begin, It&& end) -> binary_reader<std::decay_t<It>>;

template<typename Array>
binary_reader(Array&& array) -> binary_reader<std::decay_t<decltype(std::begin(array))>>;

template<std::output_iterator<char> It>
struct binary_writer final {
    explicit constexpr binary_writer(It it) noexcept;
    template<integer T> constexpr void write(T x) noexcept;
    template<std::floating_point T> void write(T x) noexcept;
    constexpr void write_asciiz_string(StringView str) noexcept;

    constexpr std::size_t bytes_written() const noexcept { return _bytes_written; }

private:
    It it;
    std::size_t _bytes_written;
};

template<std::output_iterator<char> It, serializable T>
constexpr binary_writer<It>& operator<<(binary_writer<It>& writer, T x) noexcept;

} // namespace floormat::Serialize
