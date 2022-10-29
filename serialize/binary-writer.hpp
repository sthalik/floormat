#pragma once
#include "binary-serializer.hpp"
#include <iterator>
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize {

template<std::output_iterator<char> It>
struct binary_writer final {
    explicit constexpr binary_writer(It it) noexcept;
    template<serializable T> constexpr void write(T x) noexcept;
    constexpr void write_asciiz_string(StringView str) noexcept;
    constexpr std::size_t bytes_written() const noexcept { return _bytes_written; }

private:
    It it;
    std::size_t _bytes_written;
};

template<std::output_iterator<char> It, serializable T>
constexpr binary_writer<It>& operator<<(binary_writer<It>& writer, T x) noexcept;

} // namespace floormat::Serialize
