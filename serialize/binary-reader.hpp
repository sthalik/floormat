#pragma once
#include "binary-serializer.hpp"
#include <iterator>

namespace floormat::Serialize {

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

template<string_input_iterator It>
struct binary_reader final {
    template<char_sequence Seq> explicit constexpr binary_reader(const Seq& seq) noexcept;
    constexpr binary_reader(It begin, It end) noexcept;
    constexpr void assert_end() noexcept;

    template<integer T> constexpr value_u read_u() noexcept;
    template<std::floating_point T> constexpr value_u read_u() noexcept;
    template<typename T> T read() noexcept;
    template<std::size_t N> constexpr std::array<char, N> read() noexcept;
    constexpr std::size_t bytes_read() const noexcept { return num_bytes_read; }

private:
    std::size_t num_bytes_read = 0;
    It it, end;
};

template<string_input_iterator It, serializable T>
binary_reader<It>& operator>>(binary_reader<It>& reader, T& x) noexcept;

template<string_input_iterator It> binary_reader(It&& begin, It&& end) -> binary_reader<std::decay_t<It>>;

template<typename Array>
binary_reader(Array&& array) -> binary_reader<std::decay_t<decltype(std::begin(array))>>;

} // namespace floormat::Serialize

