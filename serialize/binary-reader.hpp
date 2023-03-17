#pragma once
#include "binary-serializer.hpp"
#include <array>
#include <iterator>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

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
    constexpr void assert_end() noexcept(false);

    constexpr std::size_t bytes_read() const noexcept { return num_bytes_read; }
    template<serializable T> constexpr T read() noexcept(false);
    template<std::size_t N> constexpr std::array<char, N> read() noexcept(false);
    template<std::size_t Max> constexpr auto read_asciiz_string() noexcept(false);

    binary_reader(binary_reader&&) noexcept = default;
    binary_reader& operator=(binary_reader&&) noexcept = default;
    binary_reader(const binary_reader&) = delete;
    binary_reader& operator=(const binary_reader&) = delete;

    constexpr char peek() const;

private:
    std::size_t num_bytes_read = 0;
    It it, end;
};

template<string_input_iterator It, serializable T>
constexpr void operator<<(T& x, binary_reader<It>& reader) noexcept(false);

template<string_input_iterator It, serializable T>
constexpr void operator>>(binary_reader<It>& reader, T& x) noexcept(false);

template<string_input_iterator It> binary_reader(It&& begin, It&& end) -> binary_reader<std::decay_t<It>>;

template<typename Array>
binary_reader(const Array& array) -> binary_reader<std::decay_t<decltype(std::begin(array))>>;

} // namespace floormat::Serialize
